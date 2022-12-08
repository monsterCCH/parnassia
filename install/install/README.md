## 安装docker、kvm运行环境执行脚本
```shell
./install.sh
```
安装完成会输出相关信息到控制体, 如果环境中已经存在docker、virt-install的执行程序，会输出warn信息表示当前环境已经存在相关组件，不再安装对应组件。

## 探针组件运行

### 启动
`./parnassia.sh start`

### 停止
`./parnassia.sh stop`

### 重启
`./parnassia.sh restart`

### 添加开机自启
`./parnassia.sh install`
