# XdAleth : An Aleth Fork Running in XDU

It might be quite different from Aleth in the future, and suitable for students in universities to play with.  
Might include some really awsome features such as customized decentralized compute, p2p encrypted massaging, etc.  

Stay tuned.  

## 与原生以太坊的区别
### 交易结构修改
在原有交易结构的基础上添加了 extraMsg 字段，类型为 String 。该字段被设计用于随交易直接传输任何信息。  
我总怀疑这玩意可能因为尺寸问题爆炸  


#### 完成扩展：
Interface::submitTransaction  
JsonHelper::toTransactonSkeleton  
core/Common::TransactionSkeleton  
TransactionBase 添加extraMsg存储字段，新增输出函数，修改human-readable重载运算符  
TransactionBase 从 TransactionSkeleton 构造交易的构造器添加字段初始化功能  

（下面忘了一大堆，反正解析之类的也改了） 
JsonHelper中的解析交易的工具也改了，加了新的字段  

### 各种瞎猜+结论等
关于RLP：从 web3.js 的文档来看，应该是在 web3.js 中有一套用于序列化交易的函数。  
~~总觉得 aleth 客户端应该也有一套序列化用的函数，可以直接从 ts 转到 rlp，但目前还没有见到。~~  
~~高度怀疑：客户端中的 signTransation 自己就具有签名并序列化的功能，返回的是一个签名完了的 ts。该函数返回值应该可以直接作为参数调用 sendRawTransaction.不过好像签名了必然就成了RLP了这也很自然地说。。。~~  
嗯猜对了，signTransaction自带 RLP 功能，具体实现在 streamRLP，这是 Transaction 的一个成员函数。  


这玩意边写边忘啊。。。。。  

## 开发日志
### 2018.10.5
【未测试】完成了通过 IPC 调用 eth_sendTransaction 为入口的，从 json 到 TransactionSkeleton 转化模块的修改；  
【未测试】完成了使用 TransactionSkeleton 构造 Transaction 的模块修改；  
【未测试】完成了通过 IPC 调用 eth_signTransaction 为入口的，将 json 交易转化为签名后 RLP 的模块的修改；  
还有一些已经想不起来了的和上面配套的东西。。。  
发送交易还剩下一个 sendRawTransaction，我估计那个没啥要改的了（FLAG），要是居然果真没得改那么发送部分就全都写完了  
附修改文件的目录  
$git status   
>	modified:   README.md  
>	modified:   libethcore/Common.h  
>	modified:   libethcore/TransactionBase.cpp  
>	modified:   libethcore/TransactionBase.h  
>	modified:   libethereum/Interface.cpp  
>	modified:   libethereum/Interface.h  
>	modified:   libethereum/Transaction.h  
>	modified:   libweb3jsonrpc/Eth.cpp  
>	modified:   libweb3jsonrpc/Eth.h  
>	modified:   libweb3jsonrpc/JsonHelper.cpp  

### 2018.10.6  
【未测试】修改了解析之类的东西，加了一堆注释，从 web3 查交易获取交易 json 的解析也扩展了  
做了个编译测试，炸了  
