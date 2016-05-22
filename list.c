#include "list.h"

#include <stdio.h>

List* list_init() {
    List *l = malloc(sizeof(List));
    l->head = NULL;
    return l;
}

void list_insert(List* l, void* value) {
    if (l->head == NULL) {
        l->head = malloc(sizeof(Node));
        l->head->value = value;
        l->head->next = NULL;
    } else {
        Node* current = l->head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = malloc(sizeof(Node));
        current->next->value = value;
        current->next->next = NULL;
    }
}
