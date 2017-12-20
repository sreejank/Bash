#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "stack.h"

struct elt {
    struct elt *next;
    char *value;
};

/* push a new value onto top of stack */
void
stackPush(Stack *s, char * value)
{
    struct elt *e;

    e = malloc(sizeof(struct elt));
    assert(e);

    e->value = value;
    e->next = *s;
    *s = e;
}

int
stackEmpty(const Stack *s)
{
    return (*s == 0);
}

char *stackTop(Stack *s) {
    char *ret=(*s)->value;
    return ret;
}

char *stackPop(Stack *s)
{
    char *ret;
    struct elt *e;

    assert(!stackEmpty(s));

    ret = (*s)->value;

    /* patch out first element */
    e = *s;
    *s = e->next;

    free(e);

    return ret;
}

void stackPrint(Stack *s) {
    if(s!=NULL) {
        struct elt *e=*s;
        while(e) {
            if(e->next==NULL) {
                printf("%s",e->value);
            }
            else {
                printf("%s ",e->value);
            }
            e=e->next;
        }
        printf("\n");
    }
}
