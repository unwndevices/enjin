#include <enjin2/core/object_collection.hpp>
#include <cstdio>
#include <cstring>
using namespace enjin2;

static int passes = 0;
static int failures = 0;

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { fprintf(stderr, "FAIL: %s\n", msg); failures++; } \
        else { printf("PASS: %s\n", msg); passes++; } \
    } while(0)

// OBJ-01: setName / getName
static void test_obj01_name() {
    Object obj;
    ASSERT(obj.getName() == nullptr,          "OBJ-01: default getName() == nullptr");

    obj.setName("player");
    ASSERT(obj.getName() != nullptr,          "OBJ-01: getName() non-null after setName");
    ASSERT(strcmp(obj.getName(), "player") == 0, "OBJ-01: getName() == \"player\"");

    obj.setName(nullptr);
    ASSERT(obj.getName() == nullptr,          "OBJ-01: getName() == nullptr after setName(nullptr)");
}

// OBJ-02: ObjectCollection::findByName
static void test_obj02_findByName() {
    ObjectCollection col;

    Object* p = col.addObject<Object>();
    p->setName("player");

    Object* found = col.findByName("player");
    ASSERT(found != nullptr,                  "OBJ-02: findByName(\"player\") != nullptr");
    ASSERT(found == p,                        "OBJ-02: findByName(\"player\") == correct Object*");

    Object* notFound = col.findByName("ghost");
    ASSERT(notFound == nullptr,               "OBJ-02: findByName(\"ghost\") == nullptr");

    Object* nullResult = col.findByName(nullptr);
    ASSERT(nullResult == nullptr,             "OBJ-02: findByName(nullptr) == nullptr");

    // Object with name=nullptr should not match
    Object* unnamed = col.addObject<Object>();
    // unnamed has name=nullptr by default
    Object* shouldBeNull = col.findByName("ghost");
    ASSERT(shouldBeNull == nullptr,           "OBJ-02: unnamed object not matched by findByName");
}

// OBJ-03: addTag / hasTag / getTagCount / clearTags
static void test_obj03_tags() {
    Object obj;
    ASSERT(obj.getTagCount() == 0,            "OBJ-03: default getTagCount() == 0");

    bool r1 = obj.addTag("enemy");
    ASSERT(r1 == true,                        "OBJ-03: addTag(\"enemy\") returns true");
    ASSERT(obj.hasTag("enemy") == true,       "OBJ-03: hasTag(\"enemy\") == true");

    bool r2 = obj.addTag("collidable");
    ASSERT(r2 == true,                        "OBJ-03: addTag(\"collidable\") returns true");
    ASSERT(obj.hasTag("collidable") == true,  "OBJ-03: hasTag(\"collidable\") == true");
    ASSERT(obj.hasTag("enemy") == true,       "OBJ-03: hasTag(\"enemy\") still true after second addTag");
    ASSERT(obj.getTagCount() == 2,            "OBJ-03: getTagCount() == 2 after 2 tags");

    // Fill up to 8 tags (we already have 2, add 6 more)
    bool r3 = obj.addTag("tag3");
    bool r4 = obj.addTag("tag4");
    bool r5 = obj.addTag("tag5");
    bool r6 = obj.addTag("tag6");
    bool r7 = obj.addTag("tag7");
    bool r8 = obj.addTag("tag8");
    ASSERT(r3 && r4 && r5 && r6 && r7 && r8, "OBJ-03: tags 3-8 all return true");
    ASSERT(obj.getTagCount() == 8,            "OBJ-03: getTagCount() == 8 after 8 tags");

    // 9th tag must fail
    bool r9 = obj.addTag("tag9");
    ASSERT(r9 == false,                       "OBJ-03: 9th addTag returns false");
    ASSERT(obj.getTagCount() == 8,            "OBJ-03: getTagCount() still 8 after failed addTag");
    ASSERT(obj.hasTag("tag9") == false,       "OBJ-03: hasTag(\"tag9\") == false (not stored)");

    // clearTags
    obj.clearTags();
    ASSERT(obj.getTagCount() == 0,            "OBJ-03: getTagCount() == 0 after clearTags");
    ASSERT(obj.hasTag("enemy") == false,      "OBJ-03: hasTag(\"enemy\") == false after clearTags");
}

// OBJ-04: ObjectCollection::findAllWithTag
static void test_obj04_findAllWithTag() {
    ObjectCollection col;

    Object* obj1 = col.addObject<Object>();
    obj1->addTag("enemy");

    Object* obj2 = col.addObject<Object>();
    obj2->addTag("enemy");
    obj2->addTag("item");

    Object* obj3 = col.addObject<Object>();
    obj3->addTag("item");

    Object* buf[10] = {};
    size_t count;

    count = col.findAllWithTag("enemy", buf, 10);
    ASSERT(count == 2,                        "OBJ-04: findAllWithTag(\"enemy\") == 2");
    // Both enemy objects must be in the buffer
    bool hasObj1 = (buf[0] == obj1 || buf[1] == obj1);
    bool hasObj2 = (buf[0] == obj2 || buf[1] == obj2);
    ASSERT(hasObj1,                           "OBJ-04: buf contains obj1 (enemy)");
    ASSERT(hasObj2,                           "OBJ-04: buf contains obj2 (enemy+item)");

    count = col.findAllWithTag("item", buf, 10);
    ASSERT(count == 2,                        "OBJ-04: findAllWithTag(\"item\") == 2");

    count = col.findAllWithTag("ghost", buf, 10);
    ASSERT(count == 0,                        "OBJ-04: findAllWithTag(\"ghost\") == 0");

    // maxResults limit honored
    count = col.findAllWithTag("enemy", buf, 1);
    ASSERT(count == 1,                        "OBJ-04: findAllWithTag with maxResults=1 returns 1");
}

int main() {
    printf("=== named_objects_test ===\n");
    test_obj01_name();
    test_obj02_findByName();
    test_obj03_tags();
    test_obj04_findAllWithTag();
    printf("\n=== Results: %d passed, %d failed ===\n", passes, failures);
    return (failures == 0) ? 0 : 1;
}
