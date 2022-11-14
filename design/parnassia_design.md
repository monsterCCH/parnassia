# parnassia(梅花草)设计

## 目的

实现获取系统相关信息定时上报 redis 集群

## 设计

### 功能清单

* 启动上报宿主机信息
* 查询宿主机相关信息
* 查询docker相关信息 docker info
* 查询容器相关信息
* 查询镜像相关信息
* 文件透传

### 功能设计要点

* 上报宿主信息只在启动时上报一次
* 机器IP在多块网卡时的处理
* 磁盘使用率以文件系统挂载还是物理硬盘
* 支持 redis 集群
* docker 信息获取方式，shell命令或者其他

### 依赖基础组件

* nlohmann json
* hiredis-cluster
* gtest
* libssh2
