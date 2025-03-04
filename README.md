# brmaster

# Зачем?

В пакете `iproute2` совершенно никуда не годящееся отображение интерфейсов, которые включены в бридж.
Например:
```
$ bridge link
10: dm1: <BROADCAST,NOARP> mtu 1500 master br1 state disabled priority 32 cost 100
11: dm2: <BROADCAST,NOARP> mtu 1500 master br1 state disabled priority 32 cost 100
12: dm3: <BROADCAST,NOARP> mtu 1500 master br2 state disabled priority 32 cost 100
13: dm4: <BROADCAST,NOARP> mtu 1500 master br2 state disabled priority 32 cost 100
14: dm5: <BROADCAST,NOARP> mtu 1500 master br3 state disabled priority 32 cost 100
```

Я знаю, что люди пользуются `brctl`:
```
$ brctl show
bridge name	bridge id		STP enabled	interfaces
br1		8000.224de67b2d58	no		dm1
							dm2
br2		8000.9eee27635e9a	no		dm3
							dm4
br3		8000.860666b048d8	no		dm5
```

Я хочу написать свою утилиту, которая будет:
1. использовать протокол Netlink
2. отображать минимум информации в краткой и понятной форме.

# Алгоритм
В целом, идея достаточно простая: сделать дамп всех интерфейсов, из которого выделить:
+ бриджи
+ интерфейсы, которые в него включены

В коде я хочу видеть нечто такое:
```
[bridge 1] -- [bridge 2] -- ... -- [bridge N]
 [slave]       [slave]              [slave]
 [slave]       [slave]              [slave]
 [slave]       [slave]              [slave]
```
Это массив структур, внутри каждой из которых будет список:
```C
struct iface {
	char * name;
	list * slaves
}
```

Переменная `slaves` будет хранить в себе имена включенных в бридж интерфейсов.

Получаем такой алгоритм:
1. Делаем дамп всех интерфейсов
2. Если интерфейс включен в какой-то бридж:
	1. ищем этот бридж
		1. Если находим, то сохраняем интерфейс в список `slaves` этого бриджа.
		2. Если нет, то создаем новую ячейку бриджа и делаем то же, что в предыдущем пункте.
3. Если этот интерфейс - бридж:
	1. ищем его в массиве.
		1. Если не находим, то создаем новую ячейку
