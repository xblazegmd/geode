#pragma once

#include <optional>
#include <string>

#include <Geode/ui/Popup.hpp>
#include "../GeodeStyle.hpp"

using namespace geode::prelude;

class SocialsPopup final : public GeodePopup {
protected:
    std::optional<std::string> m_youtube;
    std::optional<std::string> m_twitter;
    std::optional<std::string> m_github;
    std::optional<std::string> m_discord;

    bool init(
        std::optional<std::string> youtube,
        std::optional<std::string> twitter,
        std::optional<std::string> github,
        std::optional<std::string> discord
    );

    void onYoutube(CCObject*);
    void onTwitter(CCObject*);
    void onGithub(CCObject*);
    void onDiscord(CCObject*);
public:
    static SocialsPopup* create(
        std::optional<std::string> youtube,
        std::optional<std::string> twitter,
        std::optional<std::string> github,
        std::optional<std::string> discord
    );
};