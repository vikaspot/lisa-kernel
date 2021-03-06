/*
 * trace_export.c - export basic ftrace utilities to user space
 *
 * Copyright (C) 2009 Steven Rostedt <srostedt@redhat.com>
 */
#include <linux/stringify.h>
#include <linux/kallsyms.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/ftrace.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>

#include "trace_output.h"

#undef TRACE_SYSTEM
#define TRACE_SYSTEM	ftrace

/* not needed for this file */
#undef __field_struct
#define __field_struct(type, item)

#undef __field
#define __field(type, item)				type item;

#undef __field_desc
#define __field_desc(type, container, item)		type item;

#undef __array
#define __array(type, item, size)			type item[size];

#undef __array_desc
#define __array_desc(type, container, item, size)	type item[size];

#undef __dynamic_array
#define __dynamic_array(type, item)			type item[];

#undef F_STRUCT
#define F_STRUCT(args...)				args

#undef F_printk
#define F_printk(fmt, args...) fmt, args

#undef FTRACE_ENTRY
#define FTRACE_ENTRY(name, struct_name, id, tstruct, print)	\
struct ____ftrace_##name {					\
	tstruct							\
};								\
static void __always_unused ____ftrace_check_##name(void)	\
{								\
	struct ____ftrace_##name *__entry = NULL;		\
								\
	/* force compile-time check on F_printk() */		\
	printk(print);						\
}

#undef FTRACE_ENTRY_DUP
#define FTRACE_ENTRY_DUP(name, struct_name, id, tstruct, print)	\
	FTRACE_ENTRY(name, struct_name, id, PARAMS(tstruct), PARAMS(print))

#include "trace_entries.h"


#undef __field
#define __field(type, item)						\
	ret = trace_seq_printf(s, "\tfield:" #type " " #item ";\t"	\
			       "offset:%zu;\tsize:%zu;\tsigned:%u;\n",	\
			       offsetof(typeof(field), item),		\
			       sizeof(field.item), is_signed_type(type)); \
	if (!ret)							\
		return 0;

#undef __field_desc
#define __field_desc(type, container, item)				\
	ret = trace_seq_printf(s, "\tfield:" #type " " #item ";\t"	\
			       "offset:%zu;\tsize:%zu;\tsigned:%u;\n",	\
			       offsetof(typeof(field), container.item),	\
			       sizeof(field.container.item),		\
			       is_signed_type(type));			\
	if (!ret)							\
		return 0;

#undef __array
#define __array(type, item, len)					\
	ret = trace_seq_printf(s, "\tfield:" #type " " #item "[" #len "];\t" \
			       "offset:%zu;\tsize:%zu;\tsigned:%u;\n",	\
			       offsetof(typeof(field), item),		\
			       sizeof(field.item), is_signed_type(type)); \
	if (!ret)							\
		return 0;

#undef __array_desc
#define __array_desc(type, container, item, len)			\
	ret = trace_seq_printf(s, "\tfield:" #type " " #item "[" #len "];\t" \
			       "offset:%zu;\tsize:%zu;\tsigned:%u;\n",	\
			       offsetof(typeof(field), container.item),	\
			       sizeof(field.container.item),		\
			       is_signed_type(type));			\
	if (!ret)							\
		return 0;

#undef __dynamic_array
#define __dynamic_array(type, item)					\
	ret = trace_seq_printf(s, "\tfield:" #type " " #item ";\t"	\
			       "offset:%zu;\tsize:0;\tsigned:%u;\n",	\
			       offsetof(typeof(field), item),		\
			       is_signed_type(type));			\
	if (!ret)							\
		return 0;

#undef F_printk
#define F_printk(fmt, args...) "%s, %s\n", #fmt, __stringify(args)

#undef __entry
#define __entry REC

#undef FTRACE_ENTRY
#define FTRACE_ENTRY(name, struct_name, id, tstruct, print)		\
static int								\
ftrace_format_##name(struct ftrace_event_call *unused,			\
		     struct trace_seq *s)				\
{									\
	struct struct_name field __attribute__((unused));		\
	int ret = 0;							\
									\
	tstruct;							\
									\
	trace_seq_printf(s, "\nprint fmt: " print);			\
									\
	return ret;							\
}

#include "trace_entries.h"

#undef __field
#define __field(type, item)						\
	ret = trace_define_field(event_call, #type, #item,		\
				 offsetof(typeof(field), item),		\
				 sizeof(field.item),			\
				 is_signed_type(type), FILTER_OTHER);	\
	if (ret)							\
		return ret;

#undef __field_desc
#define __field_desc(type, container, item)	\
	ret = trace_define_field(event_call, #type, #item,		\
				 offsetof(typeof(field),		\
					  container.item),		\
				 sizeof(field.container.item),		\
				 is_signed_type(type), FILTER_OTHER);	\
	if (ret)							\
		return ret;

#undef __array
#define __array(type, item, len)					\
	BUILD_BUG_ON(len > MAX_FILTER_STR_VAL);				\
	ret = trace_define_field(event_call, #type "[" #len "]", #item,	\
				 offsetof(typeof(field), item),		\
				 sizeof(field.item), 0, FILTER_OTHER);	\
	if (ret)							\
		return ret;

#undef __array_desc
#define __array_desc(type, container, item, len)			\
	BUILD_BUG_ON(len > MAX_FILTER_STR_VAL);				\
	ret = trace_define_field(event_call, #type "[" #len "]", #item,	\
				 offsetof(typeof(field),		\
					  container.item),		\
				 sizeof(field.container.item), 0,	\
				 FILTER_OTHER);				\
	if (ret)							\
		return ret;

#undef __dynamic_array
#define __dynamic_array(type, item)

#undef FTRACE_ENTRY
#define FTRACE_ENTRY(name, struct_name, id, tstruct, print)		\
int									\
ftrace_define_fields_##name(struct ftrace_event_call *event_call)	\
{									\
	struct struct_name field;					\
	int ret;							\
									\
	tstruct;							\
									\
	return ret;							\
}

#include "trace_entries.h"

static int ftrace_raw_init_event(struct ftrace_event_call *call)
{
	INIT_LIST_HEAD(&call->fields);
	return 0;
}

#undef __field
#define __field(type, item)

#undef __field_desc
#define __field_desc(type, container, item)

#undef __array
#define __array(type, item, len)

#undef __array_desc
#define __array_desc(type, container, item, len)

#undef __dynamic_array
#define __dynamic_array(type, item)

#undef FTRACE_ENTRY
#define FTRACE_ENTRY(call, struct_name, type, tstruct, print)		\
									\
struct ftrace_event_call __used						\
__attribute__((__aligned__(4)))						\
__attribute__((section("_ftrace_events"))) event_##call = {		\
	.name			= #call,				\
	.id			= type,					\
	.system			= __stringify(TRACE_SYSTEM),		\
	.raw_init		= ftrace_raw_init_event,		\
	.show_format		= ftrace_format_##call,			\
	.define_fields		= ftrace_define_fields_##call,		\
};									\

#include "trace_entries.h"
