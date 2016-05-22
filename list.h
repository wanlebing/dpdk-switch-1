#ifndef LIST_H
#define LIST_H

#include <stdlib.h>

typedef struct Node {
    void* value;
    struct Node* next;
} Node;

typedef struct List {
    Node* head;
} List;

List* list_init();
void list_insert(List* l, void* value);

#endif /* LIST_H */
