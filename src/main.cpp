#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/RetryLevelLayer.hpp>

using namespace geode::prelude;

class $modify(NoAutoRetryPlusPlayLayer, PlayLayer) {
public:
    struct Fields {
        bool deathScreenShown = false;
    };

private:
    bool isPracticeDeathScreenEnabled() const {
        auto mod = Mod::get();
        if (!mod) return true;
        if (!mod->hasSetting("practice-death-screen")) return true;
        return mod->getSettingValue<bool>("practice-death-screen");
    }


    bool isPracticeCursorForced() const {
        auto mod = Mod::get();
        if (!mod) return true;
        if (!mod->hasSetting("force-practice-cursor")) return true;
        return mod->getSettingValue<bool>("force-practice-cursor");
    }

public:
    void onPracticeDeathDelay(float dt) {
        if (!isPracticeDeathScreenEnabled()) {
            m_fields->deathScreenShown = false;
            return;
        }

        if (!m_isPracticeMode || !m_fields->deathScreenShown) return;
        if (!m_player1 || !m_player1->m_isDead) {
            m_fields->deathScreenShown = false;
            return;
        }

        auto layer = RetryLevelLayer::create();
        if (!layer) {
            m_fields->deathScreenShown = false;
            return;
        }

        this->addChild(layer, 100);
        layer->enterLayer();
        if (isPracticeCursorForced()) {
            if (auto director = cocos2d::CCDirector::sharedDirector()) {
                if (auto gl = director->getOpenGLView()) {
                    gl->showCursor(true);
                }
            }
        }
    }

    void resetLevel() {
        this->unschedule(
            schedule_selector(NoAutoRetryPlusPlayLayer::onPracticeDeathDelay)
        );
        m_fields->deathScreenShown = false;
        PlayLayer::resetLevel();
    }

    void resetLevelFromStart() {
        this->unschedule(
            schedule_selector(NoAutoRetryPlusPlayLayer::onPracticeDeathDelay)
        );
        m_fields->deathScreenShown = false;
        PlayLayer::resetLevelFromStart();
    }

    void onQuit() {
        this->unschedule(
            schedule_selector(NoAutoRetryPlusPlayLayer::onPracticeDeathDelay)
        );
        m_fields->deathScreenShown = false;
        PlayLayer::onQuit();
    }

    void togglePracticeMode(bool on) {
        this->unschedule(
            schedule_selector(NoAutoRetryPlusPlayLayer::onPracticeDeathDelay)
        );
        m_fields->deathScreenShown = false;
        PlayLayer::togglePracticeMode(on);
    }

    void showRetryLayer() {

        if (m_player1 && !m_player1->m_isDead) {
            return;
        }

        if (!isPracticeDeathScreenEnabled()) {
            PlayLayer::showRetryLayer();
            return;
        }

        if (m_fields->deathScreenShown) {
            return;
        }

        m_fields->deathScreenShown = true;

        if (m_isPracticeMode) {

            this->scheduleOnce(
                schedule_selector(NoAutoRetryPlusPlayLayer::onPracticeDeathDelay),
                0.4f
            );
        } else {

            PlayLayer::showRetryLayer();
        }
    }


    void delayedResetLevel() {
        if (m_isPracticeMode &&
            isPracticeDeathScreenEnabled() &&
            m_fields->deathScreenShown) {

            return;
        }

        PlayLayer::delayedResetLevel();
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        PlayLayer::destroyPlayer(player, object);
        if (m_isPracticeMode &&
            isPracticeDeathScreenEnabled() &&
            !m_fields->deathScreenShown) {
            this->showRetryLayer();
        }
    }
};

class $modify(NoAutoRetryPlusRetryLevelLayer, RetryLevelLayer) {
public:
    void keyDown(cocos2d::enumKeyCodes key) override {
        auto mod = Mod::get();
        bool enableUpArrow = true;  // default ON

        if (mod && mod->hasSetting("up-arrow-retry")) {
            enableUpArrow = mod->getSettingValue<bool>("up-arrow-retry");
        }

        if (enableUpArrow && key == cocos2d::enumKeyCodes::KEY_Up) {
            key = cocos2d::enumKeyCodes::KEY_Space;
        }

        RetryLevelLayer::keyDown(key);
    }
};

