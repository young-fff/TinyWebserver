
# 接口

* insertElement（插入数据）
* deleteElement（删除数据）
* searchElement（查询数据）
* changeElement（修改数据）
* displayList（展示已存数据）
* dumpFile（数据落盘）
* loadFile（加载数据）
* size（返回数据规模）

# 单独运行方式
```
cd skiplist_kv
make
./bin/main
```


# 存储引擎数据表现

## 插入操作

跳表树高：18 

采用随机插入数据测试：


|插入数据规模（万条） |耗时（秒） | 
|---|---|
|10 |0.31 |
|50 |1.86 |
|100 |4.10 |



## 取数据操作

|取数据规模（万条） |耗时（秒） | 
|---|---|
|10|0.47 |
|50|2.56|
|100|5.43 |



可以运行如下脚本测试kv存储引擎的性能（当然你可以根据自己的需求进行修改）

```
sh ./skiplist_kv/stress-test/stress_test_start.sh 
```


