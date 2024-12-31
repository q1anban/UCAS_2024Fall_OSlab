# UCAS2024秋季学期OSlab

本项目为中国科学院大学 **2024年秋季学期** OSLab内容，基本完成了A-core等级的要求，主要内容包括

* bootloader
* 一个简易的shell
* 进程与线程管理
* 虚拟内存的支持
* 对e1000网卡的驱动
* 文件系统
>在项目文件内我注释掉了net有关部分，因为在没有装载网卡时，对目标的访问会直接导致宕机

~~用尽洪荒之力的~~作者对该项目的完成程度为

- [x] S core的全部内容，包括上面的基本内容
- [x] A core的绝大部分内容，包括网卡中断，虚拟内存中的再分配
- [x] 自己觉得应当做的内容，比如shell中的"↑"，文件系统对相对、绝对路径的健壮支持
- [ ] C core的内容，比如文件cache，brk等
- [ ] bug的完全修复，我没遇到就是没有

感谢[learn MarkDown in Y minutes](https://learnxinyminutes.com/markdown/)对本文件的大力支持