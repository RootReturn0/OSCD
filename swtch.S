# Context switch
#
#   void swtch(struct context **old, struct context *new);
# 
# Save the current registers on the stack, creating
# a struct context, and save its address in *old.
# Switch stacks to new and pop previously-saved registers.

.globl swtch
swtch:
  # 从栈中弹出参数，放入寄存器
  movl 4(%esp), %eax
  movl 8(%esp), %edx

  # Save old callee-saved registers 保存旧的被调用者保存的寄存器，压入栈中。在当前栈上建立一个新的上下文结构
  pushl %ebp
  pushl %ebx
  pushl %esi
  pushl %edi

  # Switch stacks 切换栈，将指向新上下文的指针放入栈指针中
  movl %esp, (%eax)
  movl %edx, %esp

  # Load new callee-saved registers 载入新的被调用者保存的寄存器，即曾经的上下文
  popl %edi
  popl %esi
  popl %ebx
  popl %ebp
  ret
