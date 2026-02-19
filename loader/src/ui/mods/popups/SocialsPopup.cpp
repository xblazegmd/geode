#include "SocialsPopup.hpp"

#include <optional>
#include <string>

#include <Geode/ui/GeodeUI.hpp>
#include <Geode/utils/web.hpp>
#include "../GeodeStyle.hpp"

SocialsPopup* SocialsPopup::create(
    std::optional<std::string> youtube,
    std::optional<std::string> twitter,
    std::optional<std::string> github,
    std::optional<std::string> discord
) {
    auto ret = new SocialsPopup();
    if (ret->init(std::move(youtube), std::move(twitter), std::move(github), std::move(discord))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool SocialsPopup::init(
    std::optional<std::string> youtube,
    std::optional<std::string> twitter,
    std::optional<std::string> github,
    std::optional<std::string> discord
) {
    if (!GeodePopup::init(390.f, 240.f, GeodePopupStyle::Default)) return false;

    this->setTitle("Socials");
    m_noElasticity = true;

    m_youtube = std::move(youtube);
    m_twitter = std::move(twitter);
    m_github = std::move(github);
    m_discord = std::move(discord);

    // List
    CCSize listSize = { 310.f, 160.f };
    auto list = ScrollLayer::create(listSize);
    list->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(0.f));
    list->setTouchEnabled(true);
    list->moveToTop();

    m_mainLayer->addChildAtPosition(list, Anchor::BottomLeft);

    // List items
    bool bg = false;
    for (auto social : std::initializer_list<std::tuple<
        const char*, const char*, std::optional<std::string>, SEL_MenuHandler
    >> {
        { "YouTube", "gj_ytIcon_001.png", m_youtube, menu_selector(SocialsPopup::onYoutube) },
        { "Twitter/X", "gj_twIcon_001.png", m_twitter, menu_selector(SocialsPopup::onTwitter) },
        { "GitHub", "github.png"_spr, m_github, menu_selector(SocialsPopup::onGithub) },
        { "Discord", "gj_discordIcon_001.png", m_discord, menu_selector(SocialsPopup::onDiscord)}
    }) {
        if (!std::get<2>(social).has_value()) continue;

        auto item = CCNode::create();
        item->setContentSize({ listSize.width, 40.f });

        // Background
        auto layerColor = CCLayerColor::create(ccc4(0, 0, 0, bg ? 60 : 20));
        layerColor->setContentSize({ listSize.width, 40.f });
        item->addChildAtPosition(layerColor, Anchor::Center);
        bg = !bg;

        // Title
        auto titleLayer = CCLayer::create();
        titleLayer->setContentWidth(listSize.width / 2 + 25);

        auto icon = CCSprite::createWithSpriteFrameName(std::get<1>(social));
        icon->setScale(80.f);
        titleLayer->addChild(icon);

        auto text = CCLabelBMFont::create(std::get<0>(social), "bigFont.fnt");
        text->setScale(.7f);
        titleLayer->addChild(text);

        titleLayer->setLayout(RowLayout::create()->setAxisAlignment(AxisAlignment::Start));
        item->addChildAtPosition(titleLayer, Anchor::Left);

        // "View" button
        auto btnMenu = CCMenu::create();

        auto btnSpr = createGeodeButton("View", false, GeodeButtonSprite::Default, m_forceDisableTheme);
        btnSpr->setScale(.7f);
        auto btn = CCMenuItemSpriteExtra::create(btnSpr, this, std::get<3>(social));

        btnMenu->addChild(btn);
        item->addChildAtPosition(btnMenu, Anchor::Right);

        list->m_contentLayer->addChild(item);
    }
    list->m_contentLayer->updateLayout();

    // List border
    m_mainLayer->addChildAtPosition(
        createGeodeListBorders(listSize, m_forceDisableTheme),
        Anchor::Center
    );

    return true;
}

void SocialsPopup::onYoutube(CCObject*) {
    if (m_youtube.has_value()) {
        web::openLinkInBrowser("https://youtube.com/" + m_youtube.value());
    }
}

void SocialsPopup::onTwitter(CCObject*) {
    if (m_twitter.has_value()) {
        web::openLinkInBrowser("https://x.com/" + m_twitter.value());
    }
}

void SocialsPopup::onGithub(CCObject*) {
    if (m_github.has_value()) {
        web::openLinkInBrowser("https://github.com/" + m_github.value());
    }
}

void SocialsPopup::onDiscord(CCObject*) {
    if (m_discord.has_value()) {
        FLAlertLayer::create(
            "Discord Username",
            m_discord.value(),
            "OK"
        )->show();
    }
}