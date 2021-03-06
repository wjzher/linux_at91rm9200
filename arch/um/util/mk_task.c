#include <stdio.h>
#include <kernel-offsets.h>

void print_ptr(char *name, char *type, int offset)
{
  printf("#define %s(task) ((%s *) &(((char *) (task))[%d]))\n", name, type,
	 offset);
}

void print(char *name, char *type, int offset)
{
  printf("#define %s(task) *((%s *) &(((char *) (task))[%d]))\n", name, type,
	 offset);
}

int main(int argc, char **argv)
{
  printf("/*\n");
  printf(" * Generated by mk_task\n");
  printf(" */\n");
  printf("\n");
  printf("#ifndef __TASK_H\n");
  printf("#define __TASK_H\n");
  printf("\n");
  print_ptr("TASK_REGS", "union uml_pt_regs", TASK_REGS);
  print("TASK_PID", "int", TASK_PID);
  printf("\n");
  printf("#endif\n");
  return(0);
}
