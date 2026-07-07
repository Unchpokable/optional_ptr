#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
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

struct Payload {
    int size;
};

struct Header {
    Payload* payload;
};

// Abstract on purpose: optional_ptr<Base> must only ever be reachable through
// a Derived instance, which is exactly the polymorphic-view use case.
struct Base {
    virtual ~Base() = default;
    virtual int identify() const = 0;
};

struct Derived : Base {
    int identify() const override
    {
        return 2;
    }
};

// A type that cannot be copied or assigned, used to prove that value_or/get
// never silently copy the pointee or the fallback.
struct NoCopy {
    int value;
    explicit NoCopy(int v) : value(v)
    {
    }
    NoCopy(const NoCopy&) = delete;
    NoCopy& operator=(const NoCopy&) = delete;
};

struct ChainNode {
    int value;
    ChainNode* next = nullptr;

    optr::optional_ptr<ChainNode> next_opt() const
    {
        return optr::optional_ptr<ChainNode>(next);
    }
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
    optr::optional_ptr<Widget> opt(&w);

    EXPECT_TRUE(opt.has_value());
    EXPECT_TRUE(static_cast<bool>(opt));
    EXPECT_EQ(opt.get(), &w);
}

TEST(OptionalPtrConstruction, RawNullPointerHasNoValue)
{
    Widget* raw = nullptr;
    optr::optional_ptr<Widget> opt(raw);

    EXPECT_FALSE(opt.has_value());
    EXPECT_FALSE(static_cast<bool>(opt));
    EXPECT_EQ(opt.get(), nullptr);
}

TEST(OptionalPtrConstruction, FromSharedPtrObservesSameAddress)
{
    auto shared = std::make_shared<Widget>(Widget { 2, "bolt" });
    optr::optional_ptr<Widget> opt(shared);

    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ(opt.get(), shared.get());
    // The observer does not extend lifetime: the shared_ptr still owns it.
    EXPECT_EQ(shared.use_count(), 1);
}

TEST(OptionalPtrConstruction, FromEmptySharedPtrHasNoValue)
{
    std::shared_ptr<Widget> shared;
    optr::optional_ptr<Widget> opt(shared);

    EXPECT_FALSE(opt.has_value());
}

TEST(OptionalPtrConstruction, FromUniquePtrObservesSameAddress)
{
    auto owner = std::make_unique<Widget>(Widget { 3, "nut" });
    optr::optional_ptr<Widget> opt(owner);

    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ(opt.get(), owner.get());
}

TEST(OptionalPtrConstruction, FromEmptyUniquePtrHasNoValue)
{
    std::unique_ptr<Widget> owner;
    optr::optional_ptr<Widget> opt(owner);

    EXPECT_FALSE(opt.has_value());
}

TEST(OptionalPtrConstruction, DirectNullptrLiteralConstructsEmpty)
{
    // With the pointer_type constructor now the only viable candidate for a
    // nullptr_t argument, users can spell an empty optr::optional_ptr directly.
    optr::optional_ptr<Widget> opt(nullptr);

    EXPECT_FALSE(opt.has_value());
}

TEST(OptionalPtrConstruction, BoolConversionIsExplicit)
{
    // operator bool is explicit, so it can't silently decay into arithmetic
    // contexts (e.g. summing "how many are set" by accident).
    static_assert(!std::is_convertible_v<optr::optional_ptr<Widget>, bool>, "optr::optional_ptr's bool conversion must stay explicit");
    static_assert(
        std::is_constructible_v<bool, optr::optional_ptr<Widget>>, "optr::optional_ptr must still be explicitly testable as a bool");
}

// ===================== Access =====================

TEST(OptionalPtrAccess, DereferenceReturnsUnderlyingObject)
{
    Widget w { 4, "washer" };
    optr::optional_ptr<Widget> opt(&w);

    EXPECT_EQ(&*opt, &w);
    EXPECT_EQ((*opt).id, 4);
}

TEST(OptionalPtrAccess, ArrowOperatorReachesMembers)
{
    Widget w { 5, "screw" };
    optr::optional_ptr<Widget> opt(&w);

    EXPECT_EQ(opt->id, 5);
    EXPECT_EQ(opt->name, "screw");
}

TEST(OptionalPtrAccess, MutationThroughOptionalPtrAffectsOriginal)
{
    Widget w { 6, "clip" };
    optr::optional_ptr<Widget> opt(&w);

    opt->name = "renamed-clip";
    opt.value().id = 60;

    EXPECT_EQ(w.name, "renamed-clip");
    EXPECT_EQ(w.id, 60);
}

TEST(OptionalPtrAccess, ValueThrowsOnNullptr)
{
    Widget* raw = nullptr;
    optr::optional_ptr<Widget> opt(raw);

    EXPECT_THROW((void)opt.value(), optr::bad_optional_ptr_access);
}

TEST(OptionalPtrAccess, DereferenceThrowsOnNullptr)
{
    Widget* raw = nullptr;
    optr::optional_ptr<Widget> opt(raw);

    EXPECT_THROW((void)*opt, optr::bad_optional_ptr_access);
}

TEST(OptionalPtrAccess, ArrowThrowsOnNullptr)
{
    Widget* raw = nullptr;
    optr::optional_ptr<Widget> opt(raw);

    EXPECT_THROW((void)opt->id, optr::bad_optional_ptr_access);
}

TEST(OptionalPtrAccess, BoolConversionGuardsAccessInIfStatement)
{
    Widget w { 7, "pin" };
    Widget* nullRaw = nullptr;
    optr::optional_ptr<Widget> present(&w);
    optr::optional_ptr<Widget> absent(nullRaw);

    if(present) {
        SUCCEED();
    }
    else {
        FAIL() << "expected optr::optional_ptr wrapping a live object to be truthy";
    }

    if(absent) {
        FAIL() << "expected optr::optional_ptr wrapping nullptr to be falsy";
    }
}

TEST(OptionalPtrAccess, ConstPointeeExposesConstReference)
{
    const Widget w { 8, "rivet" };
    optr::optional_ptr<const Widget> opt(&w);

    static_assert(std::is_same_v<decltype(opt.value()), const Widget&>, "value() must not strip constness from the pointee");
    EXPECT_EQ(opt->id, 8);
}

// ===================== Real-world lookup pattern =====================

TEST(OptionalPtrRegistryUseCase, FindExistingWidgetWrapsNonNull)
{
    std::vector<Widget> widgets { { 1, "a" }, { 2, "b" }, { 3, "c" } };

    optr::optional_ptr<Widget> found(find_widget(widgets, 2));

    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->name, "b");
}

TEST(OptionalPtrRegistryUseCase, FindMissingWidgetWrapsNull)
{
    std::vector<Widget> widgets { { 1, "a" } };

    optr::optional_ptr<Widget> found(find_widget(widgets, 999));

    EXPECT_FALSE(found.has_value());
    EXPECT_THROW((void)found.value(), optr::bad_optional_ptr_access);
}

TEST(OptionalPtrRegistryUseCase, SafelyReadNameOrFallbackUsingHasValue)
{
    std::vector<Widget> widgets { { 1, "a" } };

    optr::optional_ptr<Widget> found(find_widget(widgets, 999));

    std::string name = found.has_value() ? found->name : std::string("<missing>");

    EXPECT_EQ(name, "<missing>");
}

// ===================== and_then: safe chained traversal =====================

TEST(OptionalPtrAndThen, ChainsThroughLinkedListNodes)
{
    LinkedNode third { 3, nullptr };
    LinkedNode second { 2, &third };
    LinkedNode first { 1, &second };

    optr::optional_ptr<LinkedNode> head(&first);

    auto hop = [](LinkedNode* n) {
        return optr::optional_ptr<LinkedNode>(n->next);
    };

    auto result = head.and_then(hop).and_then(hop);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->value, 3);
}

TEST(OptionalPtrAndThen, ShortCircuitsWhenNextIsNull)
{
    LinkedNode last { 1, nullptr };
    optr::optional_ptr<LinkedNode> head(&last);

    auto hop = [](LinkedNode* n) {
        return optr::optional_ptr<LinkedNode>(n->next);
    };

    // Walking past the tail must not dereference a null `next` pointer.
    auto oneHop = head.and_then(hop);
    EXPECT_FALSE(oneHop.has_value());

    auto twoHops = oneHop.and_then(hop);
    EXPECT_FALSE(twoHops.has_value());
}

TEST(OptionalPtrAndThen, DoesNotInvokeCallableWhenSourceIsNull)
{
    LinkedNode* raw = nullptr;
    optr::optional_ptr<LinkedNode> empty(raw);

    int invocationCount = 0;
    auto hop = [&invocationCount](LinkedNode* n) {
        ++invocationCount;
        return optr::optional_ptr<LinkedNode>(n->next); // would crash if ever reached
    };

    auto result = empty.and_then(hop);

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(invocationCount, 0);
}

TEST(OptionalPtrAndThen, TransformsToDifferentPointeeType)
{
    Payload payload { 128 };
    Header header { &payload };
    optr::optional_ptr<Header> opt(&header);

    // Maps optr::optional_ptr<Header> -> optr::optional_ptr<Payload>. This used to fail
    // to compile: the empty branch of and_then had to construct a private
    // nullptr_t optr::optional_ptr<Payload>, which optr::optional_ptr<Header>'s member
    // function had no access to (different class instantiations don't share
    // private access, even for the same template).
    auto result = opt.and_then([](Header* h) {
        return optr::optional_ptr<Payload>(h->payload);
    });

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size, 128);
}

TEST(OptionalPtrAndThen, TransformToDifferentPointeeTypeShortCircuitsOnNull)
{
    Header header { nullptr };
    optr::optional_ptr<Header> opt(&header);

    auto result = opt.and_then([](Header* h) {
        return optr::optional_ptr<Payload>(h->payload);
    });

    EXPECT_FALSE(result.has_value());
}

// ===================== or_else: fallback objects =====================

TEST(OptionalPtrOrElse, ProvidesFallbackNodeWhenNull)
{
    LinkedNode fallback { -1, nullptr };
    LinkedNode* raw = nullptr;
    optr::optional_ptr<LinkedNode> missing(raw);

    auto result = missing.or_else([&fallback]() {
        return optr::optional_ptr<LinkedNode>(&fallback);
    });

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.get(), &fallback);
    EXPECT_EQ(result->value, -1);
}

TEST(OptionalPtrOrElse, DoesNotInvokeFallbackWhenSourceHasValue)
{
    LinkedNode node { 42, nullptr };
    LinkedNode fallback { -1, nullptr };
    optr::optional_ptr<LinkedNode> present(&node);

    int invocationCount = 0;
    auto result = present.or_else([&]() {
        ++invocationCount;
        return optr::optional_ptr<LinkedNode>(&fallback);
    });

    EXPECT_EQ(invocationCount, 0);
    EXPECT_EQ(result.get(), &node);
}

TEST(OptionalPtrChaining, AndThenOrElseCombinedFindOrDefault)
{
    LinkedNode second { 2, nullptr };
    LinkedNode first { 1, &second };
    LinkedNode fallback { 0, nullptr };

    optr::optional_ptr<LinkedNode> head(&first);
    auto hop = [](LinkedNode* n) {
        return optr::optional_ptr<LinkedNode>(n->next);
    };

    // 1 -> 2 -> (end) -> fallback
    auto result = head.and_then(hop).and_then(hop).or_else([&fallback]() {
        return optr::optional_ptr<LinkedNode>(&fallback);
    });

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->value, 0);
}

// ===================== Copy behavior =====================

TEST(OptionalPtrCopySemantics, CopyObservesSameAddress)
{
    Widget w { 9, "washer" };
    optr::optional_ptr<Widget> original(&w);
    optr::optional_ptr<Widget> copy = original;

    EXPECT_EQ(copy.get(), original.get());
    EXPECT_EQ(copy.get(), &w);
}

TEST(OptionalPtrCopySemantics, AssignmentRebindsObservedAddress)
{
    Widget a { 10, "a" };
    Widget b { 11, "b" };
    optr::optional_ptr<Widget> opt(&a);

    opt = optr::optional_ptr<Widget>(&b);

    EXPECT_EQ(opt.get(), &b);
    EXPECT_EQ(opt->name, "b");
}

TEST(OptionalPtrCopySemantics, MoveConstructionObservesSameAddress)
{
    Widget w { 12, "moved" };
    optr::optional_ptr<Widget> original(&w);
    optr::optional_ptr<Widget> moved(std::move(original));

    EXPECT_EQ(moved.get(), &w);
}

TEST(OptionalPtrCopySemantics, MoveAssignmentObservesSameAddress)
{
    Widget a { 13, "a" };
    Widget b { 14, "b" };
    optr::optional_ptr<Widget> src(&a);
    optr::optional_ptr<Widget> dst(&b);

    dst = std::move(src);

    EXPECT_EQ(dst.get(), &a);
}

TEST(OptionalPtrCopySemantics, SelfAssignmentIsSafe)
{
    Widget w { 15, "self" };
    optr::optional_ptr<Widget> opt(&w);
    optr::optional_ptr<Widget>& selfRef = opt; // dodge -Wself-assign, still the same object

    opt = selfRef;

    EXPECT_EQ(opt.get(), &w);
}

// ===================== Polymorphism / converting constructors =====================

TEST(OptionalPtrConstruction, DefaultConstructedIsEmpty)
{
    optr::optional_ptr<Widget> opt;

    EXPECT_FALSE(opt.has_value());
    EXPECT_EQ(opt.get(), nullptr);
}

TEST(OptionalPtrConstruction, ConstructFromDerivedRawPointerUpcasts)
{
    Derived d;
    optr::optional_ptr<Base> opt(&d);

    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ(opt->identify(), 2); // virtual dispatch through Base*, no slicing
}

TEST(OptionalPtrConstruction, ConstructFromOptionalPtrOfDerivedUpcasts)
{
    Derived d;
    optr::optional_ptr<Derived> derivedOpt(&d);
    optr::optional_ptr<Base> baseOpt(derivedOpt);

    EXPECT_EQ(baseOpt.get(), static_cast<Base*>(&d));
}

TEST(OptionalPtrConstruction, ConstructFromSharedPtrOfDerivedUpcasts)
{
    auto shared = std::make_shared<Derived>();
    optr::optional_ptr<Base> opt(shared);

    EXPECT_EQ(opt.get(), static_cast<Base*>(shared.get()));
}

TEST(OptionalPtrConstruction, ConstructFromUniquePtrOfDerivedUpcasts)
{
    auto owner = std::make_unique<Derived>();
    optr::optional_ptr<Base> opt(owner);

    EXPECT_EQ(opt.get(), static_cast<Base*>(owner.get()));
}

TEST(OptionalPtrConstruction, ConstructFromNonConstPointerToConstPointeeType)
{
    Widget w { 16, "const-view" };
    optr::optional_ptr<const Widget> opt(&w);

    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ(opt->id, 16);
}

// ===================== Compile-time safety: rejected constructions =====================
// These bodies only contain static_asserts: the interesting part is that the
// program compiles at all. is_constructible_v is reliable here specifically
// because the guard constructors are `= delete` (a deleted candidate makes
// initialization ill-formed without needing its body instantiated) rather than
// a static_assert in the body (which only fires once actually called).

TEST(OptionalPtrCompileTimeSafety, RvalueSharedPtrOfExactTypeIsRejected)
{
    static_assert(!std::is_constructible_v<optr::optional_ptr<Widget>, std::shared_ptr<Widget>>,
        "constructing from an rvalue shared_ptr<T> would dangle once the temporary is destroyed");
}

TEST(OptionalPtrCompileTimeSafety, RvalueSharedPtrOfDerivedTypeIsRejected)
{
    // Regression test: before the guard constructors were templated on U, an
    // rvalue shared_ptr<Derived> silently bound to the `const shared_ptr<U>&`
    // converting constructor instead of being rejected, leaving a dangling
    // pointer once the temporary shared_ptr<Derived> was destroyed.
    static_assert(!std::is_constructible_v<optr::optional_ptr<Base>, std::shared_ptr<Derived>>,
        "an rvalue shared_ptr<Derived> must be rejected when constructing optional_ptr<Base> too");
}

TEST(OptionalPtrCompileTimeSafety, RvalueUniquePtrOfExactTypeIsRejected)
{
    static_assert(!std::is_constructible_v<optr::optional_ptr<Widget>, std::unique_ptr<Widget>>,
        "constructing from an rvalue unique_ptr<T> would dangle once the temporary is destroyed");
}

TEST(OptionalPtrCompileTimeSafety, RvalueUniquePtrOfDerivedTypeIsRejected)
{
    static_assert(!std::is_constructible_v<optr::optional_ptr<Base>, std::unique_ptr<Derived>>,
        "an rvalue unique_ptr<Derived> must be rejected when constructing optional_ptr<Base> too");
}

TEST(OptionalPtrCompileTimeSafety, UnrelatedPointerTypeIsRejected)
{
    static_assert(!std::is_constructible_v<optr::optional_ptr<Widget>, int*>,
        "optional_ptr<Widget> must not be constructible from an unrelated pointer type");
}

TEST(OptionalPtrCompileTimeSafety, UnrelatedOptionalPtrTypeIsRejected)
{
    static_assert(!std::is_constructible_v<optr::optional_ptr<Widget>, optr::optional_ptr<int>>,
        "optional_ptr<Widget> must not be constructible from optional_ptr<int>");
}

// ===================== Access edge cases =====================

TEST(OptionalPtrAccess, BadOptionalPtrAccessIsCatchableAsStdException)
{
    optr::optional_ptr<Widget> empty(nullptr);

    try {
        (void)empty.value();
        FAIL() << "expected optr::bad_optional_ptr_access to be thrown";
    }
    catch(const std::exception& e) {
        EXPECT_STREQ(e.what(), "Null pointer dereference");
    }
}

// ===================== Const-correctness =====================

TEST(OptionalPtrConstCorrectness, ConstOptionalPtrAllowsMutationOfNonConstPointee)
{
    Widget w { 17, "shallow-const" };
    const optr::optional_ptr<Widget> opt(&w);

    // The optional_ptr itself is const, but that says nothing about the
    // pointee: this is the same shallow-const behavior as a raw T*.
    opt->name = "mutated";

    EXPECT_EQ(w.name, "mutated");
}

TEST(OptionalPtrConstCorrectness, AllReadAccessorsWorkOnConstInstance)
{
    Widget w { 18, "readable" };
    const optr::optional_ptr<Widget> opt(&w);

    EXPECT_TRUE(opt.has_value());
    EXPECT_TRUE(static_cast<bool>(opt));
    EXPECT_EQ(opt.get(), &w);
    EXPECT_EQ(&*opt, &w);
    EXPECT_EQ(&opt.value(), &w);
    EXPECT_EQ(opt->id, 18);
}

// ===================== value_or =====================

TEST(OptionalPtrValueOr, ReturnsPointeeWhenPresent)
{
    Widget w { 19, "present" };
    Widget fallback { 20, "fallback" };
    optr::optional_ptr<Widget> opt(&w);

    Widget& result = opt.value_or(fallback);

    EXPECT_EQ(&result, &w);
    EXPECT_EQ(result.id, 19);
}

TEST(OptionalPtrValueOr, ReturnsFallbackWhenEmpty)
{
    // Regression test: value_or used to take its fallback by value and return
    // a reference to that (dead) parameter, i.e. a dangling reference.
    optr::optional_ptr<Widget> empty(nullptr);
    Widget fallback { 20, "fallback" };

    Widget& result = empty.value_or(fallback);

    EXPECT_EQ(&result, &fallback);
    EXPECT_EQ(result.id, 20);
}

TEST(OptionalPtrValueOr, DoesNotCopyEitherBranch)
{
    // This wouldn't compile at all if value_or copied its argument or its
    // return value anywhere along the way.
    NoCopy owned(1);
    NoCopy fallback(2);
    optr::optional_ptr<NoCopy> present(&owned);
    optr::optional_ptr<NoCopy> empty(nullptr);

    EXPECT_EQ(&present.value_or(fallback), &owned);
    EXPECT_EQ(&empty.value_or(fallback), &fallback);
}

// ===================== Equality =====================

TEST(OptionalPtrEquality, SameAddressIsEqual)
{
    Widget w { 21, "x" };
    optr::optional_ptr<Widget> a(&w);
    optr::optional_ptr<Widget> b(&w);

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
}

TEST(OptionalPtrEquality, DifferentAddressIsNotEqual)
{
    Widget a { 22, "a" };
    Widget b { 23, "b" };
    optr::optional_ptr<Widget> pa(&a);
    optr::optional_ptr<Widget> pb(&b);

    EXPECT_FALSE(pa == pb);
    EXPECT_TRUE(pa != pb);
}

TEST(OptionalPtrEquality, BothEmptyAreEqual)
{
    optr::optional_ptr<Widget> a(nullptr);
    optr::optional_ptr<Widget> b(nullptr);

    EXPECT_TRUE(a == b);
}

TEST(OptionalPtrEquality, EmptyAndNonEmptyAreNotEqual)
{
    Widget w { 24, "x" };
    optr::optional_ptr<Widget> present(&w);
    optr::optional_ptr<Widget> empty(nullptr);

    EXPECT_FALSE(present == empty);
}

TEST(OptionalPtrEquality, EqualsNullptrWhenEmpty)
{
    optr::optional_ptr<Widget> empty(nullptr);

    EXPECT_TRUE(empty == nullptr);
    EXPECT_TRUE(nullptr == empty); // rewritten candidate (C++20)
}

TEST(OptionalPtrEquality, NotEqualsNullptrWhenPresent)
{
    Widget w { 25, "x" };
    optr::optional_ptr<Widget> present(&w);

    EXPECT_FALSE(present == nullptr);
    EXPECT_TRUE(present != nullptr);
}

TEST(OptionalPtrEquality, CrossTypeComparisonViaUpcast)
{
    Derived d;
    optr::optional_ptr<Derived> derivedOpt(&d);
    optr::optional_ptr<Base> baseOpt(&d);

    EXPECT_TRUE(baseOpt == derivedOpt);
}

// ===================== Ordering =====================

TEST(OptionalPtrOrdering, EqualPointersCompareEqual)
{
    Widget w { 26, "x" };
    optr::optional_ptr<Widget> a(&w);
    optr::optional_ptr<Widget> b(&w);

    EXPECT_EQ(a <=> b, std::strong_ordering::equal);
}

TEST(OptionalPtrOrdering, OrderingIsConsistentAndAntisymmetric)
{
    Widget a { 27, "a" };
    Widget b { 28, "b" };
    optr::optional_ptr<Widget> pa(&a);
    optr::optional_ptr<Widget> pb(&b);

    auto ab = pa <=> pb;
    auto ba = pb <=> pa;

    ASSERT_NE(ab, std::strong_ordering::equal) << "distinct objects must not compare equal";
    EXPECT_EQ(ab == std::strong_ordering::less, ba == std::strong_ordering::greater);
    EXPECT_EQ(ab == std::strong_ordering::greater, ba == std::strong_ordering::less);
}

TEST(OptionalPtrOrdering, NullptrComparisonIsAntisymmetric)
{
    // Deliberately not asserting *which* direction null sorts in -- the
    // standard leaves ordering of unrelated/null pointers unspecified, this
    // only checks that the relation is self-consistent.
    Widget w { 29, "x" };
    optr::optional_ptr<Widget> present(&w);

    bool presentGtNull = present > nullptr;
    bool presentLtNull = present < nullptr;
    bool nullGtPresent = nullptr > present;
    bool nullLtPresent = nullptr < present;

    EXPECT_NE(presentGtNull, presentLtNull);
    EXPECT_EQ(presentGtNull, nullLtPresent);
    EXPECT_EQ(presentLtNull, nullGtPresent);
}

TEST(OptionalPtrOrdering, SetDeduplicatesByAddress)
{
    Widget w { 30, "x" };
    std::set<optr::optional_ptr<Widget>> s;
    s.insert(optr::optional_ptr<Widget>(&w));
    s.insert(optr::optional_ptr<Widget>(&w));

    EXPECT_EQ(s.size(), 1u);

    Widget w2 { 31, "y" };
    s.insert(optr::optional_ptr<Widget>(&w2));

    EXPECT_EQ(s.size(), 2u);
}

TEST(OptionalPtrOrdering, UsableAsMapKey)
{
    Widget a { 32, "a" };
    Widget b { 33, "b" };
    std::map<optr::optional_ptr<Widget>, std::string> m;
    m[optr::optional_ptr<Widget>(&a)] = "first";
    m[optr::optional_ptr<Widget>(&b)] = "second";

    EXPECT_EQ(m[optr::optional_ptr<Widget>(&a)], "first");
    EXPECT_EQ(m.size(), 2u);
}

TEST(OptionalPtrOrdering, SortByAddressIsTotal)
{
    Widget a { 34, "a" };
    Widget b { 35, "b" };
    Widget c { 36, "c" };
    std::vector<optr::optional_ptr<Widget>> v { optr::optional_ptr<Widget>(&c), optr::optional_ptr<Widget>(&a),
        optr::optional_ptr<Widget>(&b) };

    std::sort(v.begin(), v.end());

    for(size_t i = 0; i + 1 < v.size(); ++i) {
        EXPECT_TRUE(v[i] < v[i + 1] || v[i] == v[i + 1]);
    }
}

// ===================== swap =====================

TEST(OptionalPtrSwap, MemberSwapExchangesAddresses)
{
    Widget a { 37, "a" };
    Widget b { 38, "b" };
    optr::optional_ptr<Widget> pa(&a);
    optr::optional_ptr<Widget> pb(&b);

    pa.swap(pb);

    EXPECT_EQ(pa.get(), &b);
    EXPECT_EQ(pb.get(), &a);
}

TEST(OptionalPtrSwap, MemberSwapWithRawPointerVariable)
{
    Widget a { 39, "a" };
    Widget b { 40, "b" };
    optr::optional_ptr<Widget> opt(&a);
    Widget* raw = &b;

    opt.swap(raw);

    EXPECT_EQ(opt.get(), &b);
    EXPECT_EQ(raw, &a);
}

TEST(OptionalPtrSwap, FreeFunctionSwapViaAdl)
{
    Widget a { 41, "a" };
    Widget b { 42, "b" };
    optr::optional_ptr<Widget> pa(&a);
    optr::optional_ptr<Widget> pb(&b);

    using std::swap; // the swap idiom: let ADL prefer optr::swap over std::swap
    swap(pa, pb);

    EXPECT_EQ(pa.get(), &b);
    EXPECT_EQ(pb.get(), &a);
}

TEST(OptionalPtrSwap, SelfSwapIsNoop)
{
    Widget a { 43, "a" };
    optr::optional_ptr<Widget> pa(&a);
    optr::optional_ptr<Widget>& selfRef = pa;

    pa.swap(selfRef);

    EXPECT_EQ(pa.get(), &a);
}

TEST(OptionalPtrSwap, SwapEmptyWithNonEmpty)
{
    Widget a { 44, "a" };
    optr::optional_ptr<Widget> present(&a);
    optr::optional_ptr<Widget> empty(nullptr);

    present.swap(empty);

    EXPECT_FALSE(present.has_value());
    ASSERT_TRUE(empty.has_value());
    EXPECT_EQ(empty.get(), &a);
}

// ===================== hash =====================

TEST(OptionalPtrHash, MatchesHashOfUnderlyingPointer)
{
    Widget w { 45, "x" };
    optr::optional_ptr<Widget> opt(&w);

    std::hash<optr::optional_ptr<Widget>> optHasher;
    std::hash<Widget*> ptrHasher;

    EXPECT_EQ(optHasher(opt), ptrHasher(&w));
}

TEST(OptionalPtrHash, EqualInstancesHaveEqualHashes)
{
    Widget w { 46, "x" };
    optr::optional_ptr<Widget> a(&w);
    optr::optional_ptr<Widget> b(&w);

    ASSERT_TRUE(a == b);
    std::hash<optr::optional_ptr<Widget>> hasher;
    EXPECT_EQ(hasher(a), hasher(b));
}

TEST(OptionalPtrHash, EmptyInstancesHashConsistently)
{
    optr::optional_ptr<Widget> a(nullptr);
    optr::optional_ptr<Widget> b(nullptr);

    std::hash<optr::optional_ptr<Widget>> hasher;
    EXPECT_EQ(hasher(a), hasher(b));
}

TEST(OptionalPtrHash, UsableInUnorderedSet)
{
    Widget w { 47, "x" };
    std::unordered_set<optr::optional_ptr<Widget>> s;
    s.insert(optr::optional_ptr<Widget>(&w));
    s.insert(optr::optional_ptr<Widget>(&w));

    EXPECT_EQ(s.size(), 1u);
}

// ===================== transform =====================

TEST(OptionalPtrTransform, MapsToPointerReturnedByCallback)
{
    Payload payload { 128 };
    Header header { &payload };
    optr::optional_ptr<Header> opt(&header);

    auto result = opt.transform([](Header* h) {
        return h->payload;
    });

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->size, 128);
}

TEST(OptionalPtrTransform, ShortCircuitsOnEmptyWithoutInvokingCallback)
{
    optr::optional_ptr<Header> empty(nullptr);

    int invocationCount = 0;
    auto result = empty.transform([&](Header* h) {
        ++invocationCount;
        return h->payload; // would crash if ever reached
    });

    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(invocationCount, 0);
}

TEST(OptionalPtrTransform, ReturnsEmptyWhenCallbackYieldsNullPointer)
{
    Header header { nullptr };
    optr::optional_ptr<Header> opt(&header);

    auto result = opt.transform([](Header* h) {
        return h->payload; // Header::payload happens to be nullptr
    });

    EXPECT_FALSE(result.has_value());
}

TEST(OptionalPtrTransform, ChainsWithAndThen)
{
    Payload payload { 64 };
    Header header { &payload };
    optr::optional_ptr<Header> opt(&header);

    auto result = opt.transform([](Header* h) { return h->payload; }).and_then([](Payload* p) {
        return optr::optional_ptr<int>(&p->size);
    });

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 64);
}

TEST(OptionalPtrTransform, ChainsMultipleTransforms)
{
    int value = 7;
    optr::optional_ptr<int> opt(&value);

    auto result = opt.transform([](int* v) { return v; }).transform([](int* v) { return v; });

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 7);
}

// ===================== Edge cases =====================

TEST(OptionalPtrEdgeCases, AbstractBaseAsPointeeDispatchesVirtually)
{
    Derived d;
    optr::optional_ptr<Base> opt(&d); // Base is abstract; only reachable via Derived

    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ(opt->identify(), 2);
}

TEST(OptionalPtrEdgeCases, CallbackExceptionPropagatesThroughAndThen)
{
    Widget w { 48, "x" };
    optr::optional_ptr<Widget> opt(&w);

    EXPECT_THROW(
        opt.and_then([](Widget*) -> optr::optional_ptr<Widget> {
            throw std::runtime_error("boom");
        }),
        std::runtime_error);
}

TEST(OptionalPtrEdgeCases, AndThenAcceptsMemberFunctionPointer)
{
    ChainNode second { 2, nullptr };
    ChainNode first { 1, &second };
    optr::optional_ptr<ChainNode> head(&first);

    auto result = head.and_then(&ChainNode::next_opt);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->value, 2);
}

TEST(OptionalPtrEdgeCases, PointerToPointerPointeeType)
{
    Widget w { 49, "x" };
    Widget* raw = &w;
    optr::optional_ptr<Widget*> opt(&raw);

    ASSERT_TRUE(opt.has_value());
    EXPECT_EQ((*opt)->id, 49);
}
