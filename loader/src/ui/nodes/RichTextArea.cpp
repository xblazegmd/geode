#include "TextAreaImpl.hpp"
#include <Geode/ui/RichTextArea.hpp>
#include <Geode/ui/TextArea.hpp>
#include <iterator>
#include <memory>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

using namespace geode::prelude;

template<class T>
void RichTextKeyInstance<T>::applyChangesToSprite(cocos2d::CCFontSprite* spr) {
    if (m_key->m_applyToSprite != nullptr)
        m_key->m_applyToSprite(m_value, spr);
}

template <class T>
std::string RichTextKeyInstance<T>::runStrAddition() {
    if (m_key->m_stringAddition != nullptr)
        return m_key->m_stringAddition(m_value);
    return "";
}

template <class T>
Result<std::shared_ptr<RichTextKeyInstanceBase>> RichTextKey<T>::createInstance(const std::string& value, bool cancellation) {
    if (cancellation){
        if (value == ""){
            return Ok(std::make_shared<RichTextKeyInstance<T>>(
                RichTextKeyInstance<T>(this, T(), true))
            );
        }
        else return Err("Cancellation tags cannot have values");
    }

    auto res = m_validCheck(value);

    if (res.isErr()) return Err(res.unwrapErr());

    return Ok(std::make_shared<RichTextKeyInstance<T>>(
        RichTextKeyInstance<T>(this, res.unwrap(), false))
    );
}

class RichTextArea::Impl : public TextAreaImpl {
public:
    std::unordered_map<std::string, std::shared_ptr<RichTextKeyBase>> m_keys;
    std::unordered_map<int, std::vector<std::shared_ptr<RichTextKeyInstanceBase>>> m_instances;

    RichTextArea* m_self = nullptr;
    Impl(RichTextArea* self) : m_self(self) {};

    CCLabelBMFont* createLabel(char const* text, float top);
    float calculateOffset(CCLabelBMFont* label);
    void charIteration(geode::FunctionRef<CCLabelBMFont*(CCLabelBMFont* line, char c, float top)> overflowHandling);
    void updateLinesNoWrap();
    void updateLinesWordWrap(bool spaceWrap);
    void updateLinesCutoffWrap();
    void updateContainer();

    void addDefaultRichKeys();
    void formatText();
};

CCLabelBMFont* RichTextArea::Impl::createLabel(char const* text, float top) {
    if (m_maxLines && m_lines.size() >= m_maxLines) {
        CCLabelBMFont* last = m_lines.at(m_maxLines - 1);
        std::string_view textv = last->getString();

        last->setString(fmt::format("{}...", textv.substr(0, textv.size() - 3)).c_str());

        return nullptr;
    } else {
        CCLabelBMFont* label = CCLabelBMFont::create(text, m_font.c_str());

        label->setScale(m_scale);
        label->setPosition({ 0, top });
        label->setColor({ m_color.r, m_color.g, m_color.b });
        label->setOpacity(m_color.a);

        return label;
    }
}

// There's 1000% a better way than just copy-pasting this thing but I can't find it
// It may just be a clangd issue and I'm getting scared for no reason tho
float RichTextArea::Impl::calculateOffset(CCLabelBMFont* label) {
    return m_linePadding + label->getContentSize().height * m_scale;
}

void RichTextArea::Impl::charIteration(geode::FunctionRef<CCLabelBMFont*(CCLabelBMFont* line, char c, float top)> overflowHandling) {
    float top = 0;
    m_lines.clear();
    CCLabelBMFont* line = createLabel("", top);
    m_lines = { line };

    std::unordered_map<std::string, std::shared_ptr<RichTextKeyInstanceBase>> appliedInstances;
    int idx = 0;
    for (const char c : m_text) {
        for (const auto instance : m_instances[idx]) {
            if (appliedInstances.contains(instance->getKey())) {
                if (instance->isCancellation()) {
                    appliedInstances.erase(instance->getKey());
                } else {
                    appliedInstances[instance->getKey()] = instance;
                }
            } else {
                appliedInstances.insert({instance->getKey(), instance});
            }
        }
        idx++;

        if (c == '\n') {
            line = createLabel("", top -= calculateOffset(line));

            if (line == nullptr) {
                break;
            } else {
                m_lines.push_back(line);
            }
        } else if (m_artificialWidth && line->getContentWidth() * m_scale >= m_self->getWidth()) {
            line = overflowHandling(line, c, top -= calculateOffset(line));

            if (line == nullptr) {
                break;
            } else {
                m_lines.push_back(line);
            }
        } else {
            line->setString((std::string(line->getString()) + c).c_str());
        }

        if (line->getChildren()->lastObject() != nullptr) {
            for (const auto& [key, instance] : appliedInstances) {
                instance->applyChangesToSprite(static_cast<cocos2d::CCFontSprite*>(line->getChildren()->lastObject()));
            }
        }
    }
}

void RichTextArea::Impl::updateLinesNoWrap() {
    std::stringstream stream(m_text);
    std::string part;
    float top = 0;
    m_lines.clear();

    while (std::getline(stream, part)) {
        CCLabelBMFont* line = createLabel(part.c_str(), top);

        if (line == nullptr) {
            break;
        } else {
            top -= calculateOffset(line);

            m_lines.push_back(line);
        }
    }
}

void RichTextArea::Impl::updateLinesWordWrap(bool spaceWrap) {
    charIteration([this, spaceWrap](CCLabelBMFont* line, char c, float top) {
        const std::string_view delimiters(spaceWrap ? " " : " `~!@#$%^&*()-_=+[{}];:'\",<.>/?\\|");

        if (delimiters.find(c) == std::string_view::npos) {
            const std::string& text = line->getString();
            const size_t position = text.find_last_of(delimiters) + 1;
            CCLabelBMFont* newLine = createLabel((text.substr(position) + c).c_str(), top);

            if (newLine != nullptr) {
                line->setString(text.substr(0, position).c_str());
            }

            return newLine;
        } else {
            return createLabel(std::string(c, c != ' ').c_str(), top);
        }
    });
}

void RichTextArea::Impl::updateLinesCutoffWrap() {
    charIteration([this](CCLabelBMFont* line, char c, float top) {
        const std::string& text = line->getString();
        const char back = text.back();
        const bool lastIsSpace = back == ' ';
        CCLabelBMFont* newLine = createLabel(std::string(!lastIsSpace, back).append(std::string(c != ' ', c)).c_str(), top);

        if (newLine == nullptr && !lastIsSpace) {
            if (text[text.size() - 2] == ' ') {
                line->setString(text.substr(0, text.size() - 1).c_str());
            } else {
                line->setString((text.substr(0, text.size() - 1) + '-').c_str());
            }
        }

        return newLine;
    });
}

void RichTextArea::Impl::updateContainer() {
    switch (m_wrappingMode) {
        case NO_WRAP: {
            updateLinesNoWrap();
        } break;
        case WORD_WRAP: {
            updateLinesWordWrap(false);
        } break;
        case SPACE_WRAP: {
            updateLinesWordWrap(true);
        } break;
        case CUTOFF_WRAP: {
            updateLinesCutoffWrap();
        } break;
    }

    const size_t lineCount = m_lines.size();
    const float width = m_self->getWidth();

    if (lineCount > 0) {
        m_lineHeight = m_lines.back()->getContentSize().height * m_scale;
    } else {
        m_lineHeight = 0;
    }

    const float height = m_lineHeight * lineCount + m_linePadding * (lineCount - 1);

    m_self->setContentSize({ width, height });
    m_container->setContentSize(m_self->getContentSize());
    m_container->removeAllChildren();

    for (CCLabelBMFont* line : m_lines) {
        const float y = height + line->getPositionY();

        switch (m_alignment) {
            case kCCTextAlignmentLeft: {
                line->setAnchorPoint({ 0, 1 });
                line->setPosition({ 0, y });
            } break;
            case kCCTextAlignmentCenter: {
                line->setAnchorPoint({ 0.5f, 1 });
                line->setPosition({ width / 2, y });
            } break;
            case kCCTextAlignmentRight: {
                line->setAnchorPoint({ 1, 1 });
                line->setPosition({ width, y });
            } break;
        }

        m_container->addChild(line);
    }
}

void RichTextArea::Impl::addDefaultRichKeys() {
    m_self->addRichKey<ccColor3B>(std::make_shared<RichTextKey<ccColor3B>>(
        "color",
        [](std::string value) -> Result<ccColor3B> {
            auto colorRes = cc3bFromHexString(value);
            if (colorRes.isErr()) return Err(colorRes.unwrapErr());

            return Ok(colorRes.unwrap());
        },
        [](const ccColor3B& value, cocos2d::CCFontSprite* sprite) {
            sprite->setColor({ value.r, value.g, value.b });
        }
    ));

    m_self->addRichKey<bool>(std::make_shared<RichTextKey<bool>>(
        "flip",
        [](std::string value) -> Result<bool> {
            if (value == "") return Ok(true);

            if (value != "true" && value != "false") {
                return Err("Value must be 'true' or 'false'");
            }

            return Ok(value == "true");
        },
        [](const bool& value, cocos2d::CCFontSprite* sprite) {
            sprite->setFlipY(value);
        }
    ));

    m_self->addRichKey<bool>(std::make_shared<RichTextKey<bool>>(
        "mirror",
        [](std::string value) -> Result<bool> {
            if (value == "") return Ok(true);

            if (value != "true" && value != "false") {
                return Err("Value must be 'true' or 'false'");
            }

            return Ok(value == "true");
        },
        [](const bool& value, cocos2d::CCFontSprite* sprite) {
            sprite->setFlipX(value);
        }
    ));

    m_self->addRichKey<std::time_t>(std::make_shared<RichTextKey<std::time_t>>(
        "workingTime",
        [](std::string value) -> Result<std::time_t> {
            auto timeRes = geode::utils::numFromString<time_t>(value);
            if (timeRes.isErr()) return Ok(std::time(nullptr));

            return Ok(timeRes.unwrap());
        },
        [](const std::time_t& value) -> std::string {
            return fmt::format("{:%Y-%m-%d %H:%M:%S}", localtime(value));
        }
    ));
}

void RichTextArea::Impl::formatText() {
    std::regex pattern(R"(<(\/)?([^=<>]+)(?:\s*=\s*([^<>]+))?>)");
    std::smatch match;

    m_instances.clear();

    // theres probably a better way to do this lol idk aa
    // there probably is but I'm too dumb to find it 🫠 --xblaze
    struct MatchInfo {
        int position;
        ptrdiff_t length;
        std::string key;
        std::string value;
        int overallOffset;
        bool cancellation;
    };
    std::vector<MatchInfo> matches;


    auto begin = m_text.cbegin();
    auto end = m_text.cend();
    int offset = 0;
    while (std::regex_search(begin, end, match, pattern)) {
        int startPosition = std::distance(m_text.cbegin(), match[0].first);

        std::string value = "";
        if (match.size() >= 4 && match[3].matched) value = match[3];

        matches.push_back({ startPosition, match[0].length(), match[2], value, offset, match[1] == '/' });
        offset += match[0].length();
        begin = match.suffix().first;
    }

    std::unordered_map<int, std::vector<std::shared_ptr<RichTextKeyInstanceBase>>> instancesBeforeOffset;

    for (auto it = matches.rbegin(); it != matches.rend(); ++it) {
        const auto& m = *it;
        if (!m_keys.contains(m.key)) continue;

        auto res = m_keys[m.key]->createInstance(m.value, m.cancellation);
        if (res.isErr()) continue;

        int effectIdx = m.position - m.overallOffset;
        auto& keyRef = res.unwrap();

        if (instancesBeforeOffset.contains(effectIdx)) {
            instancesBeforeOffset[effectIdx].push_back(keyRef);
        } else {
            instancesBeforeOffset[effectIdx] = { keyRef };
        }

        m_text.erase(m.position, m.length);
    }

    int textAdditionOffset = 0;
    int prevExtraOffset = 0;

    for (const auto& [idx, keys] : instancesBeforeOffset) {
        for (const auto& keyRef : keys) {
            auto currentAddition = keyRef->runStrAddition();
            if (currentAddition == "") continue;

            m_text.insert(idx + textAdditionOffset, currentAddition);
            textAdditionOffset += currentAddition.length();
        }

        m_instances[idx + prevExtraOffset] = std::move(keys);
        prevExtraOffset = textAdditionOffset;
    }
}

RichTextArea::RichTextArea() : m_impl(std::make_unique<RichTextArea::Impl>(this)) {}
RichTextArea::~RichTextArea() = default;

bool RichTextArea::init(std::string font, std::string text, float scale, float width, const bool artificialWidth) {
    m_impl->m_font = std::move(font);
    m_impl->m_text = std::move(text);
    m_impl->m_scale = scale;
    m_impl->m_artificialWidth = artificialWidth;
    m_impl->m_container = CCMenu::create();

    m_impl->addDefaultRichKeys();
    m_impl->formatText();

    this->setAnchorPoint({ 0.5f, 0.5f });
    m_impl->m_container->setPosition({ 0, 0 });
    m_impl->m_container->setAnchorPoint({ 0, 1 });
    m_impl->m_container->setContentSize({ width, 0 });
    this->addChild(m_impl->m_container);
    m_impl->updateContainer();

    return true;
}

template<class T>
void RichTextArea::addRichKey(std::shared_ptr<RichTextKey<T>> key) {
    if (m_impl->m_keys.contains(key->getKey())) return;
    m_impl->m_keys.insert({ key->getKey(), key });
}