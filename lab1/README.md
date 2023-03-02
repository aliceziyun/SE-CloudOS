## lab1

RDT-reliable data transport（不错、不丢、不乱）

#### 0.Some change

我在`rdt_struct.h`中加入了一些宏来定义packet各字段的长度以及window的大小

并添加了一个checksum tool类，因此对readme也进行了一些修改

#### 1.Packet

packet设计如下：

```
------------------head----------------
checksum -- payload size --sequence num
----------------pay load--------------

checksum: 2bit
payload_num: 1bit
sequence_num: 4bit
```

`checksum`：校验码，将校验码放在包的第一位，这样校验码之后的地址空间是连续的

`payload_num`：记录了payload的大小（即包数据的长度）

`sequence_num`：包的序列号

#### 2.Sender & Receiver

##### 1.策略

使用Selective Repeat（选择重传）机制，在receiver方设置和sender window大小相同的缓存，当sender方发的包乱序到达时，根据packet的序号将其放入receiver缓存的对应位置，当receiver缓存的最低位有包到达时，上传包，并向sender回传ack，sender收到ack后，移动窗口继续发包。

##### 2.超时机制

###### 导致超时的场景

1. sender向receiver发送的包超时
2. receiver向sender发送的ack超时

以上两种情况都会导致sender window的下界无法收到ack。当timer超时时，重发sender window下界对应的包。

1. 若该包receiver未提交过，则提交并发送ack
2. 若该包receiver已经提交，则只向sender发送ack

###### 计时器管理

1. sender window容量满且没有计时器工作时，调用计时器
2. sender收到ack且没有计时器工作时，调用计时器
3. timer超时时，再开启新的timer，以免重发的包没有发送完成
4. sender方所有包都发送完成时，停止计时器

##### 3.其他

由于sender方收包的速率明显大于发包的速率，因此sender方需要维护一个buffer来存储未发送的包

#### 3.checksum

checksum采用16 位 Internet checksum

由于checksum算法有局限，在corrupt rate高时数据仍然可能错误

