![splash](splash.png)

* [Features](#features)
* [Examples](#examples)
* [Supported compilers](#supported-compilers)
* [License](#license)

### Features
- [x] Get a list VMs
- [x] Start/stop/restart VMs
- [x] Start/stop applications
- [x] Trigger keys, touch and swipes
- [x] Screen capture without save images
- [x] Get list a running processes of VM

### Examples
#### Get a list VMs
The ```memuc::Memuc``` provides API for interact with MEMUC (MEmu Command).
```c
memuc::Memuc memuc;
memuc.set_path("C:/Program Files/Microvirt/MEmu/");
memuc.list_vms();
```
#### Run VM and application, then trigger keys

```c
memuc::Memuc memuc;
memuc.set_path("C:/Program Files/Microvirt/MEmu/");
memuc.start_vm(0);
memuc.start_app(0, "com.myapp");
memuc.trigger_key(0, memuc::KeyCode::Back);
```

### Supported compilers
This library is Header-Only but it use functions from C++20/23 like ranges.

### License

Check LICENSE for additional information.