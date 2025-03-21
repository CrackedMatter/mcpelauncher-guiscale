#pragma once
#include <cstddef>

struct MenuEntryABI {
    const char* name;
    void*       user             = nullptr;
    bool (*selected)(void* user) = [](void*) { return false; };
    void (*click)(void* user)    = nullptr;
    size_t        length         = 0;
    MenuEntryABI* subentries     = nullptr;
};

struct ControlABI {
    int type = 0; // button
    union {
        struct {
            const char* label;
            void*       user;
            void (*onClick)(void* user);
        } button;

        struct {
            const char* label;
            int         min;
            int         def;
            int         max;
            void*       user;
            void (*onChange)(void* user, int value);
        } sliderint;

        struct {
            const char* label;
            float       min;
            float       def;
            float       max;
            void*       user;
            void (*onChange)(void* user, float value);
        } sliderfloat;

        struct {
            const char* label;
            int         size; // 0 normal, 1 small title...
        } text;

        struct {
            const char* label;
            const char* def;
            const char* placeholder;
            void*       user;
            void (*onChange)(void* user, const char* value);
        } textinput;
    } data;
};

inline void (*addMenu)(size_t length, MenuEntryABI* entries);
inline void (*showWindow)(const char* title, int isModal, void* user, void (*onClose)(void* user), int count, ControlABI* controls);
inline void (*closeWindow)(const char* title);
