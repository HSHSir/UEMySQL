# ThirdParty / MySQL Connector/C 6.1

本插件依赖 MySQL 官方 C 客户端库（libmysql），因体积与授权原因未包含在仓库中，
需要你自行下载并放置到本目录下，结构如下：

```
ThirdParty/MySQL/
├── include/            # 头文件，含 mysql.h
├── lib/
│   └── libmysql.lib    # 导入库（链接期）
└── bin/
    └── libmysql.dll    # 运行时 DLL（64 位）
```

## 下载

1. 打开 https://downloads.mysql.com/archives/c-c/
2. 选择版本 6.1.11、Microsoft Windows、64-bit
3. 下载 `mysql-connector-c-6.1.11-winx64.zip`
4. 解压后把 include/、lib/libmysql.lib、lib(或bin)/libmysql.dll 按上面结构放好

注意：必须使用 64 位版本，与 UE4 Win64 保持一致。

详细图文说明见 `../Documentation/UEMySQL使用文档.html`。
