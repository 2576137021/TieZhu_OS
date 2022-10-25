# TieZhu_OS  
这是我学习《操作系统真想还原》的完整代码和笔记记录  
代码总行数约9000行,核心代码约6000行。  
# 目录结构
  + device    
      键盘中断、时钟中断、硬盘中断、消费者生产者模式  
  + fs        
      文件系统  
  + kernel    
      位图管理、调试框架、中断向量表注册、内存管理、标准输出  
  + lib       
      系统调用  
  + shell     
      shell模块  
  + thread    
      同步锁、线程创建、多线程调度  
  + userprog  
      exec、fork、process、tss、wait、exit功能实现  
  + command   
      用户程序、简单c运行库  
  + loader.S  
      内核加载模块  
  + makefile  
      编译文件  
  + mbr.S     
      主引导程序  
# 功能展示
  ## 系统帮助:  
  
  ![image](https://user-images.githubusercontent.com/58016964/197725439-fa348fd6-b4cb-4014-b4c9-4ec1f9c5de96.png)  
  ## shell管道:  
  ![image](https://user-images.githubusercontent.com/58016964/197729317-a8f89f8f-e27d-4b7e-ae73-89b05db17e82.png)  
  
  ## cat:  
  ![image](https://user-images.githubusercontent.com/58016964/197729457-170f0335-74ce-4fbe-9727-803ac699360d.png)  
  
  ## exec:  
  ![image](https://user-images.githubusercontent.com/58016964/197729611-ebe4034e-1c3f-42e0-b2b1-f0479eddd4f7.png)  
  
  ## shell:  
  ![image](https://user-images.githubusercontent.com/58016964/197729773-866cec72-8c87-49eb-b37e-0b185137c06f.png)   
  
  ![image](https://user-images.githubusercontent.com/58016964/197729832-8bce3973-3d33-457e-9298-87b58df026df.png)

  
