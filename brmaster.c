#include <glib.h>
#include <string.h>
#include <libmnl/libmnl.h>
#include <linux/if.h>
#include <linux/rtnetlink.h>
#include <stdio.h>

typedef struct {
	int bridge_ifindex;
	char ifname[IFNAMSIZ];
	GSList *slaves;
} brgroup_t;

static int data_attr_cb2(const struct nlattr *attr, void *data)
{
	const struct nlattr **tb = data;
	tb[mnl_attr_get_type(attr)] = attr;
	return MNL_CB_OK;
}

static int data_attr_cb(const struct nlattr *attr, void *data)
{
	const struct nlattr **tb = data;
	int type = mnl_attr_get_type(attr);

	if (mnl_attr_type_valid(attr, IFLA_MAX) < 0)
		return MNL_CB_OK;

	tb[type] = attr;
	return MNL_CB_OK;
}

static bool is_link_bridge(struct nlattr **tb)
{
	if (!tb[IFLA_LINKINFO])
		return false;

	struct nlattr *tba[IFLA_INFO_MAX + 1] = { 0 };
	mnl_attr_parse_nested(tb[IFLA_LINKINFO], data_attr_cb2, tba);

	if (!tba[IFLA_INFO_KIND])
		return false;

	const char *str = mnl_attr_get_str(tba[IFLA_INFO_KIND]);

	return !strcmp(str, "bridge");
}

static bool bridge_ifindex_exists(GSList *l, int ifindex, brgroup_t **tmp)
{
	while (l) {
		*tmp = (brgroup_t *)l->data;
		if ((*tmp)->bridge_ifindex == ifindex)
			return true;
		l = l->next;
	}

	return false;
}

static int data_cb(const struct nlmsghdr *nlh, void *data)
{
	struct nlattr *tb[IFLA_MAX + 1] = { 0 };
	struct ifinfomsg *ifm = mnl_nlmsg_get_payload(nlh);

	mnl_attr_parse(nlh, sizeof(*ifm), data_attr_cb, tb);

	GSList **list = (GSList **)data;
	int br_ifindex = ifm->ifi_index;
	const char *ifname = mnl_attr_get_str(tb[IFLA_IFNAME]);
	int is_iface_bridge = is_link_bridge(tb);

	/* skip other ifaces */
	if (!tb[IFLA_MASTER] && !is_iface_bridge)
		return MNL_CB_OK;

	if (!is_iface_bridge)
		br_ifindex = mnl_attr_get_u32(tb[IFLA_MASTER]);

	brgroup_t *tmp = NULL;
	bool bridge_exists = bridge_ifindex_exists(*list, br_ifindex, &tmp);
	if (!bridge_exists) {
		tmp = calloc(1, sizeof(brgroup_t));

		if (!tmp) {
			fprintf(stderr, "No free memory left");
			abort();
		}
	}

	tmp->bridge_ifindex = br_ifindex;

	if (is_iface_bridge)
		strncpy(tmp->ifname, ifname, sizeof(tmp->ifname));
	else
		tmp->slaves = g_slist_append(tmp->slaves, strdup(ifname));

	if (!bridge_exists)
		*list = g_slist_append(*list, tmp);

	return MNL_CB_OK;
}

static void release_data(GSList **lst)
{
	GSList *l = *lst;
	while (l) {
		brgroup_t *tmp = (brgroup_t *)l->data;
		g_slist_free_full(tmp->slaves, free);
		l = l->next;
	}

	l = *lst;

	g_slist_free_full(l, free);
}

static void print_data(GSList *lst)
{
	printf("bridge <- [it's slaves]\n");
	printf("-----------------------\n");

	while (lst) {
		brgroup_t *tmp = (brgroup_t *)lst->data;
		printf("%s <- [", tmp->ifname);

		GSList *ll = tmp->slaves;
		while (ll) {
			printf("%s", (char *)ll->data);
			ll = ll->next;
			if (ll)
				printf(", ");
		}

		printf("]\n");

		lst = lst->next;
	}
}

static void print_usage(const char * name)
{
	printf("This program prints bridge and it's slaves\n");
	printf("Usage:\n");
	printf("\t%s\n", name);
}

int main(int argc, char *argv[])
{
	char buf[MNL_SOCKET_DUMP_SIZE];
	unsigned int seq, portid;
	struct mnl_socket *nl;
	struct nlmsghdr *nlh;
	struct rtgenmsg *rt;
	int ret;

	if (argc > 1)
	{
		print_usage(argv[0]);
		exit(EXIT_SUCCESS);
	}

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = RTM_GETLINK;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	nlh->nlmsg_seq = seq = time(NULL);
	rt = mnl_nlmsg_put_extra_header(nlh, sizeof(struct rtgenmsg));
	rt->rtgen_family = AF_PACKET;

	nl = mnl_socket_open(NETLINK_ROUTE);
	if (nl == NULL) {
		perror("mnl_socket_open");
		exit(EXIT_FAILURE);
	}

	if (mnl_socket_bind(nl, 0, MNL_SOCKET_AUTOPID) < 0) {
		perror("mnl_socket_bind");
		exit(EXIT_FAILURE);
	}
	portid = mnl_socket_get_portid(nl);

	if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) {
		perror("mnl_socket_sendto");
		exit(EXIT_FAILURE);
	}

	GSList *lst = NULL;

	ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
	while (ret > 0) {
		ret = mnl_cb_run(buf, ret, seq, portid, data_cb, &lst);
		if (ret <= MNL_CB_STOP)
			break;
		ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
	}

	mnl_socket_close(nl);

	if (ret == -1) {
		release_data(&lst);
		perror("mnl_socket_recvfrom");
		exit(EXIT_FAILURE);
	}

	if (!lst)
	{
		printf("no bridges!\n");
		print_usage(argv[0]);
		return EXIT_SUCCESS;
	}

	print_data(lst);
	release_data(&lst);

	return EXIT_SUCCESS;
}
