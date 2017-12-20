#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#define STACK_EMPTY (0)


typedef struct elt *Stack;
void stackPush(Stack *s, char * value);

int stackEmpty(const Stack *s);

char *stackPop(Stack *s);

void stackPrint(Stack *s);

char *stackTop(Stack *s);

