#pragma once

#include "types.h"


typedef u64 Dense_Time;

typedef enum File_Property_Flags
{
    File_Property_Is_Folder = (1 << 0),
} File_Property_Flags;

typedef struct File_Properties File_Properties;
struct File_Properties
{
    File_Property_Flags flags;
    u64                 size;
    Dense_Time          modified;
    Dense_Time          created;
};
