#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/RetryLevelLayer.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/LevelSelectLayer.hpp>
#include <Geode/modify/FLAlertLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
#include <Geode/cocos/CCDirector.h>

using namespace geode::prelude;

class $modify(NoAutoRetryPlusPlayLayer, PlayLayer) {
public:
    struct Fields {
        bool deathScreenScheduledOrShown = false;
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

    void clearDeathScreenState() {
        this->unschedule(schedule_selector(NoAutoRetryPlusPlayLayer::onPracticeDeathDelay));
        m_fields->deathScreenScheduledOrShown = false;
    }

    bool hasRetryLayerChild() {
        auto children = this->getChildren();
        if (!children) return false;

        CCObject* obj = nullptr;
        CCARRAY_FOREACH(children, obj) {
            auto node = static_cast<cocos2d::CCNode*>(obj);
            if (!node) continue;

            if (typeinfo_cast<RetryLevelLayer*>(node)) {
                return true;
            }
        }
        return false;
    }

public:
    bool createRetryLayerOnce() {
        if (!m_isPracticeMode) return false;

        if (!m_player1 || !m_player1->m_isDead) {
            return false;
        }

        auto layer = RetryLevelLayer::create();
        if (!layer) return false;

        this->addChild(layer, 100);
        layer->enterLayer();

        if (isPracticeCursorForced()) {
            if (auto director = cocos2d::CCDirector::sharedDirector()) {
                if (auto gl = director->getOpenGLView()) {
#ifdef GEODE_IS_WINDOWS
                    gl->showCursor(true);
#endif
                }
            }
        }

        return true;
    }

    void onPracticeDeathDelay(float dt) {
        if (!isPracticeDeathScreenEnabled()) {
            clearDeathScreenState();
            return;
        }

        if (!m_fields->deathScreenScheduledOrShown) {
            return;
        }

        if (!createRetryLayerOnce()) {
            this->scheduleOnce(
                schedule_selector(NoAutoRetryPlusPlayLayer::onPracticeDeathDelay),
                0.05f
            );
            return;
        }

    }

    void resetLevel() {
        clearDeathScreenState();
        PlayLayer::resetLevel();
    }

    void resetLevelFromStart() {
        clearDeathScreenState();
        PlayLayer::resetLevelFromStart();
    }

    void onQuit() {
        clearDeathScreenState();
        PlayLayer::onQuit();
    }

    void togglePracticeMode(bool on) {
        clearDeathScreenState();
        PlayLayer::togglePracticeMode(on);
    }

    void showRetryLayer() {
        if (m_player1 && !m_player1->m_isDead) {
            return;
        }

        if (!m_isPracticeMode || !isPracticeDeathScreenEnabled()) {
            if (hasRetryLayerChild()) {
                return;
            }
            PlayLayer::showRetryLayer();
            return;
        }

        if (m_fields->deathScreenScheduledOrShown) {
            return;
        }
        m_fields->deathScreenScheduledOrShown = true;

        this->scheduleOnce(
            schedule_selector(NoAutoRetryPlusPlayLayer::onPracticeDeathDelay),
            0.4f
        );
    }

    void delayedResetLevel() {
        if (m_isPracticeMode &&
            isPracticeDeathScreenEnabled() &&
            (m_fields->deathScreenScheduledOrShown || hasRetryLayerChild())) {
            return;
        }
        PlayLayer::delayedResetLevel();
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        PlayLayer::destroyPlayer(player, object);

        if (m_isPracticeMode && isPracticeDeathScreenEnabled()) {
            this->showRetryLayer();
        }
    }
};

// up arrow spacebar functionality helper

bool shouldMapUpArrow(cocos2d::enumKeyCodes key) {
    if (key != cocos2d::enumKeyCodes::KEY_Up) return false;

    auto mod = Mod::get();
    if (!mod || !mod->hasSetting("up-arrow-retry")) return true;

    return mod->getSettingValue<bool>("up-arrow-retry");
}

bool shouldRetryOnAnyClick() {
    auto mod = Mod::get();
    if (!mod || !mod->hasSetting("click-anywhere-retry")) return false;

    return mod->getSettingValue<bool>("click-anywhere-retry");
}

bool isInstantRetryEnabled() {
    auto mod = Mod::get();
    if (!mod || !mod->hasSetting("instant-retry")) return false;

    return mod->getSettingValue<bool>("instant-retry");
}

class ClickAnywhereRetryProxy : public cocos2d::CCLayer {
public:
    RetryLevelLayer* m_owner = nullptr;

    static ClickAnywhereRetryProxy* create(RetryLevelLayer* owner) {
        auto ret = new ClickAnywhereRetryProxy();
        if (ret && ret->init(owner)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool init(RetryLevelLayer* owner) {
        if (!CCLayer::init()) return false;
        m_owner = owner;
        this->setTouchEnabled(true);
        return true;
    }

    void registerWithTouchDispatcher() override {
        if (auto director = cocos2d::CCDirector::sharedDirector()) {
            if (auto dispatcher = director->getTouchDispatcher()) {
                dispatcher->addTargetedDelegate(this, -2000, true);
            }
        }
    }

    bool isTouchOverMenuButton(cocos2d::CCTouch* touch) {
        if (!m_owner) return false;
        
        auto menu = m_owner->m_mainMenu;
        if (!menu) return false;

        auto touchPos = touch->getLocation();
        
        cocos2d::CCRect menuBtnBox;
        bool found = false;

        float maxCenterX = -FLT_MAX;
        CCObject* obj = nullptr;
        CCARRAY_FOREACH(menu->getChildren(), obj) {
            auto node = static_cast<cocos2d::CCNode*>(obj);
            if (!node || !node->isVisible()) continue;

            auto box = node->boundingBox();
            auto menuPos = menu->getPosition();
            box.origin.x += menuPos.x;
            box.origin.y += menuPos.y;

            float centerX = box.getMidX();
            if (centerX > maxCenterX) {
                maxCenterX = centerX;
                menuBtnBox = box;
                found = true;
            }
        }

        if (found && menuBtnBox.containsPoint(touchPos)) {
            return true;
        }
        
        return false;
    }

    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override {
        if (!m_owner || !shouldRetryOnAnyClick()) {
            return false;
        }

        if (isTouchOverMenuButton(touch)) {
            return false;
        }

        if (isInstantRetryEnabled()) {
            m_owner->onReplay(nullptr);
        }

        return true;
    }

    void ccTouchEnded(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override {
        if (m_owner && shouldRetryOnAnyClick() && !isInstantRetryEnabled() && !isTouchOverMenuButton(touch)) {
            m_owner->onReplay(nullptr);
        }
    }
};

class InstantRetryButtonProxy : public cocos2d::CCLayer {
public:
    RetryLevelLayer* m_owner = nullptr;

    static InstantRetryButtonProxy* create(RetryLevelLayer* owner) {
        auto ret = new InstantRetryButtonProxy();
        if (ret && ret->init(owner)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool init(RetryLevelLayer* owner) {
        if (!CCLayer::init()) return false;
        m_owner = owner;
        this->setTouchEnabled(true);
        return true;
    }

    void registerWithTouchDispatcher() override {
        if (auto director = cocos2d::CCDirector::sharedDirector()) {
            if (auto dispatcher = director->getTouchDispatcher()) {
                dispatcher->addTargetedDelegate(this, -2000, true);
            }
        }
    }

    bool isTouchOverRetryButton(cocos2d::CCTouch* touch) {
        if (!m_owner || !m_owner->m_mainMenu) return false;

        auto touchPos = touch->getLocation();
        auto menu = m_owner->m_mainMenu;

        cocos2d::CCRect retryBtnBox;
        bool found = false;

        float minCenterX = FLT_MAX;
        CCObject* obj = nullptr;
        CCARRAY_FOREACH(menu->getChildren(), obj) {
            auto node = static_cast<cocos2d::CCNode*>(obj);
            if (!node || !node->isVisible()) continue;

            auto box = node->boundingBox();
            auto menuPos = menu->getPosition();
            box.origin.x += menuPos.x;
            box.origin.y += menuPos.y;

            float centerX = box.getMidX();
            if (centerX < minCenterX) {
                minCenterX = centerX;
                retryBtnBox = box;
                found = true;
            }
        }

        if (found && retryBtnBox.containsPoint(touchPos)) {
            return true;
        }

        return false;
    }

    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override {
        if (!m_owner || !isInstantRetryEnabled()) {
            return false;
        }

        if (isTouchOverRetryButton(touch)) {
            m_owner->onReplay(nullptr);
            return true;
        }

        return false;
    }
};

// up arrow spacebar functionality and click anywhere/instant retry shizzle

class $modify(NoAutoRetryPlusRetryLevelLayer, RetryLevelLayer) {
public:

    void customSetup() override {
        RetryLevelLayer::customSetup();

        bool clickAnywhereOn = shouldRetryOnAnyClick();
        bool instantRetryOn = isInstantRetryEnabled();

        if (clickAnywhereOn) {
            if (auto proxy = ClickAnywhereRetryProxy::create(this)) {
                this->addChild(proxy, 9999);
            }
        } else if (instantRetryOn) {
            if (auto proxy = InstantRetryButtonProxy::create(this)) {
                this->addChild(proxy, 9999);
            }
        }
    }

    void keyDown(cocos2d::enumKeyCodes key) override {
        if (shouldMapUpArrow(key)) {
            key = cocos2d::enumKeyCodes::KEY_Space;
        }
        RetryLevelLayer::keyDown(key);
    }

    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override {
        return RetryLevelLayer::ccTouchBegan(touch, event);
    }

    void ccTouchEnded(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override {
        RetryLevelLayer::ccTouchEnded(touch, event);
    }
};

class $modify(NoAutoRetryPlusLevelInfoLayer, LevelInfoLayer) {
public:
    void keyDown(cocos2d::enumKeyCodes key) override {
        if (shouldMapUpArrow(key)) {
            key = cocos2d::enumKeyCodes::KEY_Space;
        }
        LevelInfoLayer::keyDown(key);
    }
};

class $modify(NoAutoRetryPlusLevelSelectLayer, LevelSelectLayer) {
public:
    void keyDown(cocos2d::enumKeyCodes key) override {
        if (shouldMapUpArrow(key)) {
            key = cocos2d::enumKeyCodes::KEY_Space;
        }
        LevelSelectLayer::keyDown(key);
    }
};

class $modify(NoAutoRetryPlusEditLevelLayer, EditLevelLayer) {
public:
    void keyDown(cocos2d::enumKeyCodes key) override {
        if (shouldMapUpArrow(key)) {
            key = cocos2d::enumKeyCodes::KEY_Space;
        }
        EditLevelLayer::keyDown(key);
    }
};

class $modify(NoAutoRetryPlusFLAlertLayer, FLAlertLayer) {
public:
    void keyDown(cocos2d::enumKeyCodes key) override {
        if (shouldMapUpArrow(key)) {
            key = cocos2d::enumKeyCodes::KEY_Space;
        }
        FLAlertLayer::keyDown(key);
    }
};

class $modify(NoAutoRetryPlusMenuLayer, MenuLayer) {
public:
    void keyDown(cocos2d::enumKeyCodes key) override {
        if (shouldMapUpArrow(key)) {
            key = cocos2d::enumKeyCodes::KEY_Space;
        }
        MenuLayer::keyDown(key);
    }
};

class $modify(NoAutoRetryPlusPauseLayer, PauseLayer) {
public:
    void keyDown(cocos2d::enumKeyCodes key) override {
        if (shouldMapUpArrow(key)) {
            key = cocos2d::enumKeyCodes::KEY_Space;
        }
        PauseLayer::keyDown(key);
    }
};

class ClickAnywhereCompleteScreenProxy : public cocos2d::CCLayer {
public:
    EndLevelLayer* m_owner = nullptr;

    static ClickAnywhereCompleteScreenProxy* create(EndLevelLayer* owner) {
        auto ret = new ClickAnywhereCompleteScreenProxy();
        if (ret && ret->init(owner)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool init(EndLevelLayer* owner) {
        if (!CCLayer::init()) return false;
        m_owner = owner;
        this->setTouchEnabled(true);
        return true;
    }

    void registerWithTouchDispatcher() override {
        if (auto director = cocos2d::CCDirector::sharedDirector()) {
            if (auto dispatcher = director->getTouchDispatcher()) {
                dispatcher->addTargetedDelegate(this, -2000, true);
            }
        }
    }

    bool isTouchOverButton(cocos2d::CCTouch* touch) {
        if (!m_owner) return false;
        
        auto menu = m_owner->m_sideMenu;
        if (!menu) return false;

        auto touchPos = touch->getLocation();
        
        CCObject* obj = nullptr;
        CCARRAY_FOREACH(menu->getChildren(), obj) {
            auto node = static_cast<cocos2d::CCNode*>(obj);
            if (!node || !node->isVisible()) continue;

            auto box = node->boundingBox();
            auto menuPos = menu->getPosition();
            box.origin.x += menuPos.x;
            box.origin.y += menuPos.y;

            if (box.containsPoint(touchPos)) {
                return true;
            }
        }
        
        return false;
    }

    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override {
        if (!m_owner || !shouldRetryOnAnyClick()) {
            return false;
        }

        if (isTouchOverButton(touch)) {
            return false;
        }

        if (isInstantRetryEnabled()) {
            m_owner->onReplay(nullptr);
        }

        return true;
    }

    void ccTouchEnded(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override {
        if (m_owner && shouldRetryOnAnyClick() && !isInstantRetryEnabled() && !isTouchOverButton(touch)) {
            m_owner->onReplay(nullptr);
        }
    }
};

class InstantRetryCompleteButtonProxy : public cocos2d::CCLayer {
public:
    EndLevelLayer* m_owner = nullptr;

    static InstantRetryCompleteButtonProxy* create(EndLevelLayer* owner) {
        auto ret = new InstantRetryCompleteButtonProxy();
        if (ret && ret->init(owner)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool init(EndLevelLayer* owner) {
        if (!CCLayer::init()) return false;
        m_owner = owner;
        this->setTouchEnabled(true);
        return true;
    }

    void registerWithTouchDispatcher() override {
        if (auto director = cocos2d::CCDirector::sharedDirector()) {
            if (auto dispatcher = director->getTouchDispatcher()) {
                dispatcher->addTargetedDelegate(this, -2000, true);
            }
        }
    }

    bool isTouchOverRetryButton(cocos2d::CCTouch* touch) {
        if (!m_owner || !m_owner->m_sideMenu) return false;

        auto touchPos = touch->getLocation();
        auto menu = m_owner->m_sideMenu;

        cocos2d::CCRect retryBtnBox;
        bool found = false;

        float minCenterX = FLT_MAX;
        CCObject* obj = nullptr;
        CCARRAY_FOREACH(menu->getChildren(), obj) {
            auto node = static_cast<cocos2d::CCNode*>(obj);
            if (!node || !node->isVisible()) continue;

            auto box = node->boundingBox();
            auto menuPos = menu->getPosition();
            box.origin.x += menuPos.x;
            box.origin.y += menuPos.y;

            float centerX = box.getMidX();
            if (centerX < minCenterX) {
                minCenterX = centerX;
                retryBtnBox = box;
                found = true;
            }
        }

        if (found && retryBtnBox.containsPoint(touchPos)) {
            return true;
        }

        return false;
    }

    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override {
        if (!m_owner || !isInstantRetryEnabled()) {
            return false;
        }

        if (isTouchOverRetryButton(touch)) {
            m_owner->onReplay(nullptr);
            return true;
        }

        return false;
    }
};

class $modify(NoAutoRetryPlusEndLevelLayer, EndLevelLayer) {
public:
    void customSetup() override {
        EndLevelLayer::customSetup();

        bool clickAnywhereOn = shouldRetryOnAnyClick();
        bool instantRetryOn = isInstantRetryEnabled();
        
        if (clickAnywhereOn) {
            if (auto proxy = ClickAnywhereCompleteScreenProxy::create(this)) {
                this->addChild(proxy, 9999);
            }
        } else if (instantRetryOn) {
            if (auto proxy = InstantRetryCompleteButtonProxy::create(this)) {
                this->addChild(proxy, 9999);
            }
        }
    }

    void keyDown(cocos2d::enumKeyCodes key) override {
        if (shouldMapUpArrow(key)) {
            key = cocos2d::enumKeyCodes::KEY_Space;
        }
        EndLevelLayer::keyDown(key);
    }

    bool ccTouchBegan(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override {
        return EndLevelLayer::ccTouchBegan(touch, event);
    }

    void ccTouchEnded(cocos2d::CCTouch* touch, cocos2d::CCEvent* event) override {
        EndLevelLayer::ccTouchEnded(touch, event);
    }
};
