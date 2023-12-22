#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include "mongoc/mongoc.h"

int main(void)
{
    mongoc_client_t *client;
    mongoc_collection_t *collection;
    bson_error_t error;
    bson_oid_t oid;
    bson_t *doc;

    mongoc_init();

    client = mongoc_client_new("mongodb+srv://timberlake2025:IamMusical222@taipan-cluster-1.iieczyn.mongodb.net/?retryWrites=true&w=majority");
    collection = mongoc_client_get_collection(client, "mydb", "mycoll");

    doc = bson_new();
    bson_oid_init(&oid, NULL);
    BSON_APPEND_OID(doc, "_id", &oid);
    BSON_APPEND_UTF8(doc, "hello", "world");

    if (!mongoc_collection_insert_one(
            collection, doc, NULL, NULL, &error))
    {
        fprintf(stderr, "%s\n", error.message);
    }

    bson_destroy(doc);
    mongoc_collection_destroy(collection);
    mongoc_client_destroy(client);
    mongoc_cleanup();

    return 0;
}
