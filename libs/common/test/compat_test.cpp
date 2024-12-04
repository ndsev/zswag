#include "zswag/test_common.hpp"
#include <map>

TEST_CASE("string_view compatibility", "[compat]") {
    using zswag::compat::string_view;
    
    SECTION("basic string_view operations") {
        std::string test = "Hello World";
        string_view view(test);
        
        REQUIRE(view.size() == 11);
        REQUIRE(std::string(view.data(), view.size()) == "Hello World");
        
        auto sub = view.substr(0, 5);
        REQUIRE(std::string(sub.data(), sub.size()) == "Hello");
    }
}

TEST_CASE("optional compatibility", "[compat]") {
    using zswag::compat::optional;
    
    SECTION("basic optional operations") {
        optional<int> empty;
        REQUIRE(!empty.has_value());
        
        optional<std::string> value("test");
        REQUIRE(value.has_value());
        REQUIRE(*value == "test");
    }
    
    SECTION("optional assignment") {
        struct Complex {
            std::string str;
            int num;
        };
        
        optional<Complex> opt;
        opt = Complex{"test", 42};
        REQUIRE(opt->str == "test");
        REQUIRE(opt->num == 42);
    }
}

TEST_CASE("map_entry_helper compatibility", "[compat]") {
    using zswag::compat::map_entry_helper;
    
    std::map<std::string, int> test_map{{"one", 1}, {"two", 2}};
    auto helper = map_entry_helper<std::map<std::string, int>>(test_map.begin());
    
    REQUIRE(helper.key() == "one");
    REQUIRE(helper.value() == 1);
} 