#include <desktop/rule/layerRule/LayerRule.hpp>
#include <desktop/rule/layerRule/LayerRuleApplicator.hpp>

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

TEST(LayerRule, LegacyScreenShareBlockTakesPriorityOverOmission) {
    CLayerRuleApplicator rules{nullptr};
    EXPECT_FALSE(rules.blocksScreenShare());
    EXPECT_FALSE(rules.omitsFromScreenShare());

    for (const auto* mode : {"normal", "black", "omit"}) {
        rules.screenShareMode().set(mode, Desktop::Types::PRIORITY_WINDOW_RULE);
        for (const bool legacyBlock : {false, true}) {
            rules.noScreenShare().set(legacyBlock, Desktop::Types::PRIORITY_WINDOW_RULE);
            EXPECT_EQ(rules.blocksScreenShare(), legacyBlock || std::string_view{mode} == "black");
            EXPECT_EQ(rules.omitsFromScreenShare(), !legacyBlock && std::string_view{mode} == "omit");
        }
    }
}
