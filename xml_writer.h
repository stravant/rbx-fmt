#pragma once
#include <libxml/xmlwriter.h>

void write_rbx_file(struct rbx_file *file);
void write_object(xmlTextWriterPtr writer, struct rbx_object *object);
