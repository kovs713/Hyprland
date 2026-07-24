#include <desktop/rule/layerRule/LayerRule.hpp>

#include <gtest/gtest.h>

using namespace Desktop::Rule;

TEST(LayerRule, screenShareModeAcceptsKnownModes) {
    for (const auto* mode : {"normal", "black", "omit"}) {
        CLayerRule rule;
        const auto result = rule.addEffect(LAYER_RULE_EFFECT_SCREEN_SHARE_MODE, mode);

        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(rule.effects().size(), 1);
        EXPECT_EQ(std::get<std::string>(rule.effects().front().value), mode);
    }
}

TEST(LayerRule, screenShareModeRejectsUnknownMode) {
    CLayerRule rule;
    const auto result = rule.addEffect(LAYER_RULE_EFFECT_SCREEN_SHARE_MODE, "transparent");

    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("normal, black, omit"), std::string::npos);
    EXPECT_TRUE(rule.effects().empty());
}
