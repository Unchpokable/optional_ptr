#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "optional_ptr.hxx"

namespace
{
struct Widget {
    int id;
    std::string name;
};

struct LinkedNode {
    int value;
    LinkedNode* next = nullptr;
};

Widget* find_widget(std::vector<Widget>& widgets, int id)
{
    for(auto& w : widgets) {
        if(w.id == id) {
            return &w;
        }
    }
    return nullptr;
}
} // namespace

// ===================== Construction =====================

TEST(OptionalPtrConstruction, RawPointerToExistingObjectHasValue)
{
    Widget w { 1, "gear" };
    optional_ptr<Widget> opt(&w);

    EXPECT_TRUE(opt.has_value());
    EXPECT_TRUE(static_cast<bool>(opt));
    EXPECT_EQ(opt.get(), &w);
}

TEST(OptionalPtrConstruction, RawNullPointerHasNoValue)
{
    Widget* raw = nullptr;
    optional_ptr<Widget> opt(raw);

    EXPECT_FALSE(opt.has_value());
    EXPECT_FALSE(static_cast<bool>(opt));
    EXPECT_EQ(opt.get(), nullptr);
}

TEST(OptionalPtrConstruction, FromSharedPtrObservesSameAddress)
{
    auto shared = std::make_shared<Widget>(Widget { 2, "bolt" });
    optional_ptr<Widget> opt(shared);

    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ(opt.get(), shared.get());
    // The observer does not extend lifetime: the shared_ptr still owns it.
    EXPECT_EQ(shared.use_count(), 1);
}

TEST(OptionalPtrConstruction, FromEmptySharedPtrHasNoValue)
{
    std::shared_ptr<Widget> shared;
    optional_ptr<Widget> opt(shared);

    EXPECT_FALSE(opt.has_value());
}

TEST(OptionalPtrConstruction, FromUniquePtrObservesSameAddress)
{
    auto owner = std::make_unique<Widget>(Widget { 3, "nut" });
    optional_ptr<Widget> opt(owner);

    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ(opt.get(), owner.get());
}

TEST(OptionalPtrConstruction, FromEmptyUniquePtrHasNoValue)
{
    std::unique_ptr<Widget> owner;
    optional_ptr<Widget> opt(owner);

    EXPECT_FALSE(opt.has_value());
}

TEST(OptionalPtrConstruction, CannotBeBuiltDirectlyFromNullptrLiteral)
{
    // A user reaching for the obvious `optional_ptr<Widget> opt(nullptr);`
    // will find that the nullptr_t constructor is private - the class only
    // wants that path used internally (and_then/or_else). Callers must go
    // through a typed null pointer instead, e.g. `Widget* p = nullptr;`.
    static_assert(!std::is_constructible_v<optional_ptr<Widget>, std::nullptr_t>,
        "optional_ptr should not be publicly constructible from a bare nullptr literal");
}

TEST(OptionalPtrConstruction, BoolConversionIsExplicit)
{
    // operator bool is explicit, so it can't silently decay into arithmetic
    // contexts (e.g. summing "how many are set" by accident).
    static_assert(!std::is_convertible_v<optional_ptr<Widget>, bool>,
        "optional_ptr's bool conversion must stay explicit");
    static_assert(std::is_constructible_v<bool, optional_ptr<Widget>>,
        "optional_ptr must still be explicitly testable as a bool");
}

// ===================== Access =====================

TEST(OptionalPtrAccess, DereferenceReturnsUnderlyingObject)
{
    Widget w { 4, "washer" };
    optional_ptr<Widget> opt(&w);

    EXPECT_EQ(&*opt, &w);
    EXPECT_EQ((*opt).id, 4);
}

TEST(OptionalPtrAccess, ArrowOperatorReachesMembers)
{
    Widget w { 5, "screw" };
    optional_ptr<Widget> opt(&w);

    EXPECT_EQ(opt->id, 5);
    EXPECT_EQ(opt->name, "screw");
}

TEST(OptionalPtrAccess, MutationThroughOptionalPtrAffectsOriginal)
{
    Widget w { 6, "clip" };
    optional_ptr<Widget> opt(&w);

    opt->name = "renamed-clip";
    opt.value().id = 60;

    EXPECT_EQ(w.name, "renamed-clip");
    EXPECT_EQ(w.id, 60);
}

TEST(OptionalPtrAccess, ValueThrowsOnNullptr)
{
    Widget* raw = nullptr;
    optional_ptr<Widget> opt(raw);

    EXPECT_THROW((void)opt.value(), detail::nullpointer_dereference);
}

TEST(OptionalPtrAccess, DereferenceThrowsOnNullptr)
{
    Widget* raw = nullptr;
    optional_ptr<Widget> opt(raw);

    EXPECT_THROW((void)*opt, detail::nullpointer_dereference);
}

TEST(OptionalPtrAccess, ArrowThrowsOnNullptr)
{
    Widget* raw = nullptr;
    optional_ptr<Widget> opt(raw);

    EXPECT_THROW((void)opt->id, detail::nullpointer_dereference);
}

TEST(OptionalPtrAccess, BoolConversionGuardsAccessInIfStatement)
{
    Widget w { 7, "pin" };
    Widget* nullRaw = nullptr;
    optional_ptr<Widget> present(&w);
    optional_ptr<Widget> absent(nullRaw);

    if(present) {
        SUCCEED();
    }
    else {
        FAIL() << "expected optional_ptr wrapping a live object to be truthy";
    }

    if(absent) {
        FAIL() << "expected optional_ptr wrapping nullptr to be falsy";
    }
}

TEST(OptionalPtrAccess, ConstPointeeExposesConstReference)
{
    const Widget w { 8, "rivet" };
    optional_ptr<const Widget> opt(&w);

    static_assert(std::is_same_v<decltype(opt.value()), const Widget&>,
        "value() must not strip constness from the pointee");
    EXPECT_EQ(opt->id, 8);
}

// ===================== Real-world lookup pattern =====================

TEST(OptionalPtrRegistryUseCase, FindExistingWidgetWrapsNonNull)
{
    std::vector<Widget> widgets { { 1, "a" }, { 2, "b" }, { 3, "c" } };

    optional_ptr<Widget> found(find_widget(widgets, 2));

    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->name, "b");
}

TEST(OptionalPtrRegistryUseCase, FindMissingWidgetWrapsNull)
{
    std::vector<Widget> widgets { { 1, "a" } };

    optional_ptr<Widget> found(find_widget(widgets, 999));

    EXPECT_FALSE(found.has_value());
    EXPECT_THROW((void)found.value(), detail::nullpointer_dereference);
}

TEST(OptionalPtrRegistryUseCase, SafelyReadNameOrFallbackUsingHasValue)
{
    std::vector<Widget> widgets { { 1, "a" } };

    optional_ptr<Widget> found(find_widget(widgets, 999));

    std::string name = found.has_value() ? found->name : std::string("<missing>");

    EXPECT_EQ(name, "<missing>");
}

// ===================== and_then: safe chained traversal =====================

TEST(OptionalPtrAndThen, ChainsThroughLinkedListNodes)
{
    LinkedNode third { 3, nullptr };
    LinkedNode second { 2, &third };
    LinkedNode first { 1, &second };

    optional_ptr<LinkedNode> head(&first);

    auto hop = [](LinkedNode* n) { return optional_ptr<LinkedNode>(n->next); };

    auto result = head.and_then(hop).and_then(hop);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->value, 3);
}

TEST(OptionalPtrAndThen, ShortCircuitsWhenNextIsNull)
{
    LinkedNode last { 1, nullptr };
    optional_ptr<LinkedNode> head(&last);

    auto hop = [](LinkedNode* n) { return optional_ptr<LinkedNode>(n->next); };

    // Walking past the tail must not dereference a null `next` pointer.
    auto oneHop = head.and_then(hop);
    EXPECT_FALSE(oneHop.has_value());

    auto twoHops = oneHop.and_then(hop);
    EXPECT_FALSE(twoHops.has_value());
}

TEST(OptionalPtrAndThen, DoesNotInvokeCallableWhenSourceIsNull)
{
    LinkedNode* raw = nullptr;
    optional_ptr<LinkedNode> empty(raw);

    int invocationCount = 0;
    auto hop = [&invocationCount](LinkedNode* n) {
        ++invocationCount;
        return optional_ptr<LinkedNode>(n->next); // would crash if ever reached
    };

    auto result = empty.and_then(hop);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(invocationCount, 0);
}

// ===================== or_else: fallback objects =====================

TEST(OptionalPtrOrElse, ProvidesFallbackNodeWhenNull)
{
    LinkedNode fallback { -1, nullptr };
    LinkedNode* raw = nullptr;
    optional_ptr<LinkedNode> missing(raw);

    auto result = missing.or_else([&fallback]() { return optional_ptr<LinkedNode>(&fallback); });

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.get(), &fallback);
    EXPECT_EQ(result->value, -1);
}

TEST(OptionalPtrOrElse, DoesNotInvokeFallbackWhenSourceHasValue)
{
    LinkedNode node { 42, nullptr };
    LinkedNode fallback { -1, nullptr };
    optional_ptr<LinkedNode> present(&node);

    int invocationCount = 0;
    auto result = present.or_else([&]() {
        ++invocationCount;
        return optional_ptr<LinkedNode>(&fallback);
    });

    EXPECT_EQ(invocationCount, 0);
    EXPECT_EQ(result.get(), &node);
}

TEST(OptionalPtrChaining, AndThenOrElseCombinedFindOrDefault)
{
    LinkedNode second { 2, nullptr };
    LinkedNode first { 1, &second };
    LinkedNode fallback { 0, nullptr };

    optional_ptr<LinkedNode> head(&first);
    auto hop = [](LinkedNode* n) { return optional_ptr<LinkedNode>(n->next); };

    // 1 -> 2 -> (end) -> fallback
    auto result = head.and_then(hop).and_then(hop).or_else(
        [&fallback]() { return optional_ptr<LinkedNode>(&fallback); });

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->value, 0);
}

// ===================== Copy behavior =====================

TEST(OptionalPtrCopySemantics, CopyObservesSameAddress)
{
    Widget w { 9, "washer" };
    optional_ptr<Widget> original(&w);
    optional_ptr<Widget> copy = original;

    EXPECT_EQ(copy.get(), original.get());
    EXPECT_EQ(copy.get(), &w);
}

TEST(OptionalPtrCopySemantics, AssignmentRebindsObservedAddress)
{
    Widget a { 10, "a" };
    Widget b { 11, "b" };
    optional_ptr<Widget> opt(&a);

    opt = optional_ptr<Widget>(&b);

    EXPECT_EQ(opt.get(), &b);
    EXPECT_EQ(opt->name, "b");
}
