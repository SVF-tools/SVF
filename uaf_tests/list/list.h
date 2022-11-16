/**
 * CS 2110 - Spring 2022 - Homework 9
 *
 * Do not modify this file!
 *
 * struct list.h
 */

#ifndef LIST_H
#define LIST_H

#include <stdlib.h>
#include <string.h>

#define FAILURE 1
#define SUCCESS 0

// This is just to remove compiler warnings associated with unused variables.
// Delete calls to this as you implement functions.
#define UNUSED(x) ((void)(x))

/**************************
 ** Datatype definitions **
 **************************/

/*
 * The following structs define the struct user_list and Node for use in list.c. DO NOT MODIFY THESE STRUCT DEFINITIONS
 */

struct student
{
    int num_classes;
    double *grades; // length of grades is num_classes
};

struct instructor
{
    double salary;
};

enum user_type {
    STUDENT,
    INSTRUCTOR
};

union user_data {
  struct student student;
  struct instructor instructor;
};

struct user
{
    struct user *next; // pointer to the next node in the list
    char *name;
    enum user_type type;
    union user_data data;
};

struct user_list
{
    struct user *head; // Head pointer either points to a struct user or NULL (if the list is empty)
    struct user *tail;
    int size;   // Size of the struct list
};

/***************************************************
** Prototypes for struct list library functions.  **
**                                                **
** For more details on their functionality,       **
** check struct list.c.                           **
***************************************************/

/* Creating */
struct user_list *create_list(void);

/* Adding */
int push_front(struct user_list *list, char *name, enum user_type type, union user_data data);
int push_back(struct user_list *list, char *name, enum user_type type, union user_data data);
int add_at_index(struct user_list *list, int index, char *name, enum user_type type, union user_data data);

/* Querying */
int get(struct user_list *list, int index, struct user **dataOut);
int contains(struct user_list *list, struct user *data, struct user **dataOut);

/* Removing */
int pop_front(struct user_list *list, struct user **dataOut);
int pop_back(struct user_list *list, struct user **dataOut);
int remove_at_index(struct user_list *list, struct user **dataOut, int index);
void empty_list(struct user_list *list);

/* Analysis */
int get_highest_paid(struct user_list *list, struct user **dataOut);
int is_passing_all_classes(struct user_list *list, char *name, int *dataOut);
int end_semester(struct user_list *list);

#endif
