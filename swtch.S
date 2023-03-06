# Context switch
#
#   void swtch(struct context **old, struct context *new);
# 
# Save the current registers on the stack, creating
# a struct context, and save its address in *old.
# Switch stacks to new and pop previously-saved registers.

.globl swtch
swtch:
    #Abhijit: swtch was called through a function call.
    #So %eip was saved on stack already.
  movl 4(%esp), %eax    #Abhijit: eax=old
  movl 8(%esp), %edx    #Abhijit: ebx=new  

  # Save old callee-saved registers
  pushl %ebp
  pushl %ebx
  pushl %esi
  pushl %edi

  # Switch stacks
  movl %esp, (%eax)     #Abhijit: *eax=*(old) = current stack = {old context with 5 values on it - eip, ebp, ebx, esi, edi in reverse order} 
  movl %edx, %esp       #Abhijit: esp=new = {new context address} //this must have 5 values as a result of the previous swtch()

  # Load new callee-saved registers
  popl %edi
  popl %esi
  popl %ebx
  popl %ebp     #Abhijit: newesp = newesp - 16, context restored
  ret       #Abhijit: this will return in the context of the "new" now
