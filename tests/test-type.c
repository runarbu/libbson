/*
 * Copyright 2016 MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <bson.h>

#include "TestSuite.h"
#include "json-test.h"

#include <limits.h>

#ifdef _MSC_VER
#define PATH_MAX 1024
#define realpath(path, expanded) GetFullPathName(path, PATH_MAX, expanded, NULL)
#endif

typedef void (*test_bson_type_valid_cb)(const bson_t *input, const char *expected, const bson_t *json);

#ifdef _MSC_VER
# define SSCANF sscanf_s
#else
# define SSCANF sscanf
#endif

void
test_bson_type_int32 (const bson_t *input, const char *expected, const bson_t *json)
{
   int32_t number1, number2;
   bson_t *bson;
   bson_iter_t iter;

   BSON_ASSERT (input);
   BSON_ASSERT (expected);
   BSON_ASSERT (json);


   SSCANF (expected, "%d", &number2);

   ASSERT (bson_iter_init_find(&iter, input, "i")
		   && BSON_ITER_HOLDS_INT32(&iter));
   number1 = bson_iter_int32(&iter);

   ASSERT_CMPINT32 (number1, ==, number2);

   bson = BCON_NEW ("i", BCON_INT32 (number1));

   ASSERT_CMPSTR (bson_as_json (bson, NULL), bson_as_json (json, NULL));
   bson_free (bson);
}

void
test_bson_type_bool (const bson_t *input, const char *expected, const bson_t *json)
{
   bool b;
   bson_t *bson;
   bson_iter_t iter;

   BSON_ASSERT (input);
   BSON_ASSERT (expected);
   BSON_ASSERT (json);


   ASSERT (bson_iter_init_find(&iter, input, "b")
		   && BSON_ITER_HOLDS_BOOL(&iter));
   b = bson_iter_bool(&iter);

   ASSERT_CMPSTR (expected, b ? "true" : "false");

   bson = BCON_NEW ("b", BCON_BOOL (b));

   ASSERT_CMPSTR (bson_as_json (bson, NULL), bson_as_json (json, NULL));
   bson_free (bson);
}

void
test_bson_type_decimal128 (const bson_t *input, const char *expected, const bson_t *json)
{
   char bid_string[BSON_DECIMAL128_STRING];
   bson_t *bson;
   uint64_t low_le;
   uint64_t high_le;
   uint8_t bytes[24];
   int n;
   bson_decimal128_t d;
   bson_iter_t iter;

   BSON_ASSERT (input);
   BSON_ASSERT (expected);
   BSON_ASSERT (json);

   ASSERT (bson_iter_init_find(&iter, input, "d")
		   && BSON_ITER_HOLDS_DECIMAL128(&iter));
   ASSERT (bson_iter_decimal128(&iter, &d));

   bson_decimal128_to_string (&d, bid_string);

   ASSERT_CMPSTR (bid_string, expected);

   bson = BCON_NEW ("d", BCON_DECIMAL128 (&d));

   ASSERT_CMPSTR (bson_as_json (bson, NULL), bson_as_json (json, NULL));
   bson_free (bson);
}

static void
_test_bson_type_print_description (bson_iter_t *iter)
{
	if (bson_iter_find (iter, "description") && BSON_ITER_HOLDS_UTF8 (iter)) {
		if (test_suite_debug_output ()) {
			fprintf (stderr, "  - %s\n", bson_iter_utf8 (iter, NULL));
			fflush (stderr);
		}
	}
}

void
_test_bson_type_visit_corrupt (const bson_iter_t *iter,
               void              *data)
{
   *((bool *) data) = true;
}

#define IS_NAN(dec) (dec).high == 0x7c00000000000000ull

static void
test_bson_type (
				bson_t *scenario,
				test_bson_type_valid_cb valid)
{
   bson_iter_t iter;
   bson_iter_t inner_iter;
   BSON_ASSERT (scenario);

   if (bson_iter_init_find (&iter, scenario, "valid")) {
      const char *expected = NULL;
      bson_t json = BSON_INITIALIZER;
	  bson_t bson_input = BSON_INITIALIZER;

      bson_iter_recurse (&iter, &inner_iter);
      while (bson_iter_next (&inner_iter)) {
         bson_iter_t test;

         bson_iter_recurse (&inner_iter, &test);
		 _test_bson_type_print_description(&test);
		 if (bson_iter_find (&test, "subject") && BSON_ITER_HOLDS_UTF8 (&test)) {
			 const char *input = NULL;
			 unsigned int byte;
			 uint8_t *doc = NULL;
			 uint32_t length;
			 int x = 0;
			 int i = 0;

			 input = bson_iter_utf8 (&test, &length);
			 doc = bson_malloc (length / 2);
			 while (SSCANF(&input[i], "%2x", &byte) == 1) {
				 doc[x++] = byte;
				 i += 2;
			 }
			 if (i == length && bson_init_static (&bson_input, doc, x)) {
				 if (test_suite_debug_output ()) {
					 fprintf (stderr, "%s\n", bson_as_json (&bson_input, NULL));
					 fflush (stderr);
				 }
			 } else {
				 fprintf(stderr, "Failed to create bson from subject\n");
				 ASSERT(0);
			 }
		 }

         if (bson_iter_find (&test, "string") && BSON_ITER_HOLDS_UTF8 (&test)) {
            expected = bson_iter_utf8 (&test, NULL);
         }
         if (bson_iter_find (&test, "extjson") && BSON_ITER_HOLDS_DOCUMENT (&test)) {
            uint32_t len;
            const uint8_t *data;

            bson_iter_document (&test, &len, &data);
            assert (bson_init_static (&json, data, len));
         }
         valid (&bson_input, expected, &json);
      }
   }
   if (bson_iter_init_find (&iter, scenario, "decodeErrors")) {
	   fprintf(stderr, "Not implemented");
	   ASSERT(0);
   }
   if (bson_iter_init_find (&iter, scenario, "parseErrors")) {
      bson_iter_recurse (&iter, &inner_iter);
      while (bson_iter_next (&inner_iter)) {
         bson_iter_t test;

         bson_iter_recurse (&inner_iter, &test);
		 _test_bson_type_print_description (&test);

		 if (bson_iter_find (&test, "subject") && BSON_ITER_HOLDS_UTF8 (&test)) {
			 uint32_t length;
			 bson_decimal128_t d;
			 const char *input = bson_iter_utf8 (&test, &length);

			 ASSERT (!bson_decimal128_from_string (input, &d));
			 ASSERT (IS_NAN (d));
		 }
	  }
   }
}

static void
test_bson_type_int32_cb (bson_t *scenario)
{
   test_bson_type (scenario, test_bson_type_int32);
}

static void
test_bson_type_bool_cb (bson_t *scenario)
{
   test_bson_type (scenario, test_bson_type_bool);
}

static void
test_bson_type_decimal128_cb (bson_t *scenario)
{
   test_bson_type (scenario, test_bson_type_decimal128);
}


void
test_add_spec_test (TestSuite *suite, const char *filename, test_hook callback)
{
   bson_t *test;
   char *skip_json;

   test = get_bson_from_json_file ((char *)filename);
   skip_json = strstr(filename, "json")+4;
   skip_json = bson_strndup (skip_json, strlen(skip_json)-5);

   TestSuite_AddWC (suite, skip_json, (void (*)(void *))callback, (void (*)(void*))bson_destroy, test);
   bson_free (skip_json);
}

void
test_bson_type_install (TestSuite *suite)
{
   test_add_spec_test (suite, "tests/json/type/boolean.json", test_bson_type_bool_cb);
   test_add_spec_test (suite, "tests/json/type/int32.json", test_bson_type_int32_cb);
   test_add_spec_test (suite, "tests/json/type/decimal128.json", test_bson_type_decimal128_cb);
}

