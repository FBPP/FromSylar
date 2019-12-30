+ 转自 https://space.bilibili.com/344487288/video?tid=0&page=1&keyword=&order=pubdate

## 项目环境
bin --输出二进制
build --中间生成路径
cmake --cmake函数文件夹
CMakeLists.txt --cmake的定义文件
lib --库的输出路径
Makefile
sylar -- 源代码
test -- 测试代码

# sylar

## 日志系统
1)
	仿照Log4j
 
	Logger(定义日志类别)
		|
		|-------Formatter(日志格式)
		|
	Appender(日志输出的地方)

## 配置系统
Config --> Yaml

## 协程库的封装

## socket系列函数库开发

## http协议开发

## 分布式协议库


## 用框架写一个推荐系统