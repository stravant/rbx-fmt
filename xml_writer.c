#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "rbx_types.h"
#include "fmt_rbx.h"
#include <libxml/xmlwriter.h>
#include <libxml/xmlsave.h>
#include <libxml/parser.h>

#define FLOAT_FORMAT_SPECIFIER "%.9g"
#define STRING_ENCODING "ISO-8859-1"

/**
 * ConvertInput:
 * @in: string in a given encoding
 * @encoding: the encoding used
 *
 * Converts @in into UTF-8 for processing with libxml2 APIs
 *
 * Returns the converted UTF-8 string, or NULL in case of error.
 */
xmlChar *
ConvertInput(const char *in, const char *encoding)
{
    xmlChar *out;
    int ret;
    int size;
    int out_size;
    int temp;
    xmlCharEncodingHandlerPtr handler;

    if (in == 0)
        return 0;

    handler = xmlFindCharEncodingHandler(encoding);

    if (!handler) {
        printf("ConvertInput: no encoding handler found for '%s'\n",
               encoding ? encoding : "");
        return 0;
    }

    size = (int) strlen(in) + 1;
    out_size = size * 2 - 1;
    out = (unsigned char *) xmlMalloc((size_t) out_size);

    if (out != 0) {
        temp = size - 1;
        ret = handler->input(out, &out_size, (const xmlChar *) in, &temp);
        if ((ret < 0) || (temp - size + 1)) {
            if (ret < 0) {
                printf("ConvertInput: conversion wasn't successful.\n");
            } else {
                printf
                    ("ConvertInput: conversion wasn't successful. converted: %i octets.\n",
                     temp);
            }

            xmlFree(out);
            out = 0;
        } else {
            out = (unsigned char *) xmlRealloc(out, out_size + 1);
            out[out_size] = 0;  /*null terminating out */
        }
    } else {
        printf("ConvertInput: no mem\n");
    }

    return out;
}

void write_object(xmlTextWriterPtr writer, struct rbx_file *file, struct rbx_object *object) {
	xmlTextWriterStartElement(writer, BAD_CAST "Item");
	xmlTextWriterWriteAttribute(writer, BAD_CAST "class", object->type->name.data);
	xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "referent", "RBX%d", object->referent);
	xmlTextWriterStartElement(writer, BAD_CAST "Properties");
	
	for (int i = object->prop_value_count - 2; i >= 0; --i) {
		struct rbx_object_propentry *prop_entry = (object->prop_value_array + i);
		uint8_t type = prop_entry->prop->value_type;
		uint8_t *name = prop_entry->prop->name.data;
		struct rbx_value *value = prop_entry->value;
		
		if (0 == strcmp("Parent", (char*)name)) {
		//	printf("breaking parent\n");
			break;
		}
		
		switch (type) {
		
		case RBX_TYPE_STRING:
			xmlTextWriterStartElement(writer, BAD_CAST "string");
			xmlTextWriterWriteAttribute(writer, BAD_CAST "name", name);
			if (0 != strcmp("ClusterGridV3", (char*) name)) {
				xmlTextWriterWriteString(writer, ConvertInput( (char*) value->string_value.data, STRING_ENCODING));
			} else {
				xmlTextWriterWriteBase64(writer, (char*) value->string_value.data , 0, value->string_value.length);
			}
			xmlTextWriterEndElement(writer);
			break;
			
		case RBX_TYPE_BOOLEAN:
			xmlTextWriterStartElement(writer, BAD_CAST "bool");
			xmlTextWriterWriteAttribute(writer, BAD_CAST "name", name);
			xmlTextWriterWriteString(writer, value->boolean_value.data ? BAD_CAST "true" : BAD_CAST "false");
			xmlTextWriterEndElement(writer);
			break;
			
		case RBX_TYPE_INT32:
			xmlTextWriterStartElement(writer, BAD_CAST "int");
			xmlTextWriterWriteAttribute(writer, BAD_CAST "name", name);
			xmlTextWriterWriteFormatString(writer, "%u", value->int32_value.data);
			xmlTextWriterEndElement(writer);
			break;
			
		case RBX_TYPE_FLOAT:
			xmlTextWriterStartElement(writer, BAD_CAST "float");
			xmlTextWriterWriteAttribute(writer, BAD_CAST "name", name);
			xmlTextWriterWriteFormatString(writer, FLOAT_FORMAT_SPECIFIER, value->float_value.data);
			xmlTextWriterEndElement(writer);
			break;
			
		case RBX_TYPE_REAL:
			xmlTextWriterStartElement(writer, BAD_CAST "double");
			xmlTextWriterWriteAttribute(writer, BAD_CAST "name", name);
			xmlTextWriterWriteFormatString(writer, "%f", value->real_value.data);
			xmlTextWriterEndElement(writer);
			break;
			
		case RBX_TYPE_UDIM2:
			xmlTextWriterStartElement(writer, BAD_CAST "UDim2");
			xmlTextWriterWriteAttribute(writer, BAD_CAST "name", name);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "XS", FLOAT_FORMAT_SPECIFIER, value->udim2_value.x.scale);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "XO", "%d", value->udim2_value.x.offset);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "YS", FLOAT_FORMAT_SPECIFIER, value->udim2_value.y.scale);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "YO", "%d", value->udim2_value.y.offset);
			xmlTextWriterEndElement(writer);
			break;
			
		case RBX_TYPE_RAY:
			xmlTextWriterStartElement(writer, BAD_CAST "Ray");
			xmlTextWriterWriteAttribute(writer, BAD_CAST "name", name);
			xmlTextWriterStartElement(writer, BAD_CAST "origin");
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "X", FLOAT_FORMAT_SPECIFIER, value->ray_value.origin.x);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "Y", FLOAT_FORMAT_SPECIFIER, value->ray_value.origin.y);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "Z", FLOAT_FORMAT_SPECIFIER, value->ray_value.origin.z);
			xmlTextWriterEndElement(writer);
			xmlTextWriterStartElement(writer, BAD_CAST "direction");
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "X", FLOAT_FORMAT_SPECIFIER, value->ray_value.direction.x);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "Y", FLOAT_FORMAT_SPECIFIER, value->ray_value.direction.y);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "Z", FLOAT_FORMAT_SPECIFIER, value->ray_value.direction.z);
			xmlTextWriterEndElement(writer);
			xmlTextWriterEndElement(writer);
			break;
			
		case RBX_TYPE_FACES:
			xmlTextWriterStartElement(writer, BAD_CAST "Faces");
			xmlTextWriterWriteAttribute(writer, BAD_CAST "name", name);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "faces", "%d",	value->faces_value.right << 5
				| value->faces_value.left << 4
				| value->faces_value.top << 3
				| value->faces_value.bottom << 2
				| value->faces_value.front << 1
				| value->faces_value.back
			);
			xmlTextWriterEndElement(writer);
			break;
			
		case RBX_TYPE_AXES:
			xmlTextWriterStartElement(writer, BAD_CAST "Axes");
			xmlTextWriterWriteAttribute(writer, BAD_CAST "name", name);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "axes", "%d", value->axes_value.x << 2 | value->axes_value.y << 1 | value->axes_value.z);
			xmlTextWriterEndElement(writer);
			break;
			
		case RBX_TYPE_BRICKCOLOR:
			xmlTextWriterStartElement(writer, BAD_CAST "int");
			xmlTextWriterWriteAttribute(writer, BAD_CAST "name", name);
			xmlTextWriterWriteFormatString(writer, "%u", value->brickcolor_value.data);
			xmlTextWriterEndElement(writer);
			break;
			
		case RBX_TYPE_COLOR3:
			xmlTextWriterStartElement(writer, BAD_CAST "Color3");
			xmlTextWriterWriteAttribute(writer, BAD_CAST "name", name);
			char hex[9];
			snprintf(hex, 9, "ff%02x%02x%02x",
				(int)(value->color3_value.r * 255),
				(int)(value->color3_value.g * 255),
				(int)(value->color3_value.b * 255));
			long long int dec = strtoll(hex, NULL, 16);
			xmlTextWriterWriteFormatString(writer, "%lld", dec);
			xmlTextWriterEndElement(writer);
			break;
			
		case RBX_TYPE_VECTOR2:
			xmlTextWriterStartElement(writer, BAD_CAST "Vector2");
			xmlTextWriterWriteAttribute(writer, BAD_CAST "name", name);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "X", FLOAT_FORMAT_SPECIFIER, value->vector2_value.x);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "Y", FLOAT_FORMAT_SPECIFIER, value->vector2_value.y);
			xmlTextWriterEndElement(writer);
			break;
			
		case RBX_TYPE_VECTOR3:
			xmlTextWriterStartElement(writer, BAD_CAST "Vector3");
			xmlTextWriterWriteAttribute(writer, BAD_CAST "name", name);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "X", FLOAT_FORMAT_SPECIFIER, value->vector3_value.x);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "Y", FLOAT_FORMAT_SPECIFIER, value->vector3_value.y);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "Z", FLOAT_FORMAT_SPECIFIER, value->vector3_value.z);
			xmlTextWriterEndElement(writer);
			break;
			
		case RBX_TYPE_VECTOR3INT16:
			xmlTextWriterStartElement(writer, BAD_CAST "Vector3int16");
			xmlTextWriterWriteAttribute(writer, BAD_CAST "name", name);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "X", "%i", value->vector3int16_value.x);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "Y", "%i", value->vector3int16_value.y);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "Z", "%i", value->vector3int16_value.z);
			xmlTextWriterEndElement(writer);
			break;
			
		case RBX_TYPE_CFRAME:
			xmlTextWriterStartElement(writer, BAD_CAST "CoordinateFrame");
			xmlTextWriterWriteAttribute(writer, BAD_CAST "name", name);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "X", FLOAT_FORMAT_SPECIFIER, value->cframe_value.position.x);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "Y", FLOAT_FORMAT_SPECIFIER, value->cframe_value.position.y);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "Z", FLOAT_FORMAT_SPECIFIER, value->cframe_value.position.z);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "R00", FLOAT_FORMAT_SPECIFIER, value->cframe_value.rotation[0]);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "R01", FLOAT_FORMAT_SPECIFIER, value->cframe_value.rotation[1]);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "R02", FLOAT_FORMAT_SPECIFIER, value->cframe_value.rotation[2]);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "R10", FLOAT_FORMAT_SPECIFIER, value->cframe_value.rotation[3]);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "R11", FLOAT_FORMAT_SPECIFIER, value->cframe_value.rotation[4]);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "R12", FLOAT_FORMAT_SPECIFIER, value->cframe_value.rotation[5]);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "R20", FLOAT_FORMAT_SPECIFIER, value->cframe_value.rotation[6]);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "R21", FLOAT_FORMAT_SPECIFIER, value->cframe_value.rotation[7]);
			xmlTextWriterWriteFormatElement(writer, BAD_CAST "R22", FLOAT_FORMAT_SPECIFIER, value->cframe_value.rotation[8]);
			xmlTextWriterEndElement(writer);
			break;
			
		case RBX_TYPE_TOKEN:
			xmlTextWriterStartElement(writer, BAD_CAST "token");
			xmlTextWriterWriteAttribute(writer, BAD_CAST "name", name);
			xmlTextWriterWriteFormatString(writer, "%u", value->token_value.data);
			xmlTextWriterEndElement(writer);
			break;
			
		case RBX_TYPE_OBJECT:
			xmlTextWriterStartElement(writer, BAD_CAST "Ref");
			xmlTextWriterWriteAttribute(writer, BAD_CAST "name", name);
			if (value->object_value.data == NULL) {
				xmlTextWriterWriteString(writer, BAD_CAST "null");
			} else {
				xmlTextWriterWriteFormatString(writer, "RBX%d", ((struct rbx_object *)value->object_value.data)->referent);
			}
			xmlTextWriterEndElement(writer);
			break;
		
		default:
			printf("Unimplemented type");
			break;
		}
	}
	
	xmlTextWriterEndElement(writer); // </Properties>
	
	// performance be damned!
	for (int j = 0; j < file->object_count; ++j) {
		struct rbx_object *child = (file->object_array + j);
		struct rbx_object_propentry *prop_entry = (child->prop_value_array + child->prop_value_count - 1);
		struct rbx_object *obj = prop_entry->value->object_value.data;
		if (obj == object) {
			write_object(writer, file, child);
		}
	}
	
	xmlTextWriterEndElement(writer); // </Item>
}



void write_rbx_file(struct rbx_file *file) {
	
	xmlDocPtr doc;
	xmlTextWriterPtr writer;
	xmlSaveCtxtPtr ctxt;
	
	writer = xmlNewTextWriterDoc(&doc, 0);
		
	xmlTextWriterStartElement(writer, BAD_CAST "roblox");
	xmlTextWriterWriteAttribute(writer, BAD_CAST "xmlns:xmime", BAD_CAST "http://www.w3.org/2005/05/xmlmime");
	xmlTextWriterWriteAttribute(writer, BAD_CAST "xmlns:xsi", BAD_CAST "http://www.w3.org/2001/XMLSchema-instance");
	xmlTextWriterWriteAttribute(writer, BAD_CAST "xsi:noNamespaceSchemaLocation", BAD_CAST "http://www.roblox.com/roblox.xsd");
	xmlTextWriterWriteAttribute(writer, BAD_CAST "version", BAD_CAST "4");
	
	xmlTextWriterWriteElement(writer, BAD_CAST "External", BAD_CAST "null");
	xmlTextWriterWriteElement(writer, BAD_CAST "External", BAD_CAST "nil");
	
	for (int i = 0; i < file->object_count; ++i) {
		struct rbx_object *object = (file->object_array + i);
		struct rbx_object_propentry *prop_entry = (object->prop_value_array + object->prop_value_count - 1);
		if (prop_entry->value->object_value.data == NULL) {
			write_object(writer, file, object);
		}
	}
	xmlTextWriterEndElement(writer);
	
	xmlFreeTextWriter(writer);
	
	ctxt = xmlSaveToFilename("out.rbxl", NULL, XML_SAVE_NO_EMPTY);
	xmlSaveDoc(ctxt, doc);
	xmlSaveFlush(ctxt);	
}
