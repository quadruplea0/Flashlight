#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
using namespace geode::prelude;

class PixelTrail : public CCNode {
    struct Px { CCDrawNode* n; CCPoint v; float life, max; };
    std::vector<Px> m_px;
    float m_t = 0;
public:
    static PixelTrail* create() {
        auto n = new PixelTrail();
        if (n && n->init()) { n->autorelease(); return n; }
        CC_SAFE_DELETE(n); return nullptr;
    }
    bool init() override { if (!CCNode::init()) return false; scheduleUpdate(); return true; }
    void update(float dt) override {
        for (m_t += dt; m_t > 0.03f; m_t -= 0.03f)
            if ((int)m_px.size() < 60) {
                float s = 2.5f + (rand()%30)/10.f;
                auto* node = CCDrawNode::create();
                CCPoint sq[4] = {CCPoint(-s/2,-s/2),CCPoint(s/2,-s/2),CCPoint(s/2,s/2),CCPoint(-s/2,s/2)};
                node->drawPolygon(sq, 4, ccc4f(1.f,.52f,.05f,1.f), 0, ccc4f(0,0,0,0));
                node->setPosition(CCPoint(-8.f-(rand()%8),-8.f+(rand()%16)));
                addChild(node);
                float life = 0.25f+(rand()%30)/100.f;
                m_px.push_back({node,CCPoint(-55.f-(float)(rand()%80),-20.f+(float)(rand()%40)),life,life});
            }
        for (auto it = m_px.begin(); it != m_px.end();) {
            it->life -= dt;
            if (it->life <= 0) { it->n->removeFromParent(); it = m_px.erase(it); continue; }
            it->n->setPosition(it->n->getPosition() + it->v * dt);
            float t = it->life / it->max;
            it->n->setOpacity((GLubyte)(t*255));
            it->n->setScale(t*0.9f+0.1f);
            ++it;
        }
    }
};

class DarknessNode : public CCNode {
    CCRenderTexture* m_rt = nullptr;
    CCSprite*        m_sprite = nullptr;
    CCDrawNode*      m_draw = nullptr;
    CCSize           m_winSize;

    void drawCone(CCPoint sp, bool flip) {
        constexpr float H = 4000.f;
        const ccColor4F dk = ccc4f(0,0,0,1);
        float px = sp.x, py = sp.y, tx = px+(flip?-325.f:325.f);
        CCPoint top[5] = {CCPoint(-H,py),CCPoint(-H,py+H),CCPoint(tx+H,py+H),CCPoint(tx,py+90.f),CCPoint(px,py)};
        CCPoint bot[5] = {CCPoint(-H,py),CCPoint(px,py),CCPoint(tx,py-90.f),CCPoint(tx+H,py-H),CCPoint(-H,py-H)};
        float rx0=flip?tx-H:tx, rx1=flip?tx:tx+H;
        CCPoint right[4] = {CCPoint(rx0,py-H),CCPoint(rx0,py+H),CCPoint(rx1,py+H),CCPoint(rx1,py-H)};
        m_draw->drawPolygon(top,5,dk,0,ccc4f(0,0,0,0));
        m_draw->drawPolygon(bot,5,dk,0,ccc4f(0,0,0,0));
        m_draw->drawPolygon(right,4,dk,0,ccc4f(0,0,0,0));
    }

public:
    static DarknessNode* create() {
        auto n = new DarknessNode();
        if (n && n->init()) { n->autorelease(); return n; }
        CC_SAFE_DELETE(n); return nullptr;
    }
    bool init() override {
        if (!CCNode::init()) return false;
        m_winSize = CCDirector::sharedDirector()->getWinSize();
        m_rt = CCRenderTexture::create((int)m_winSize.width,(int)m_winSize.height,kCCTexture2DPixelFormat_RGBA8888);
        m_rt->retain();
        m_sprite = m_rt->getSprite();
        m_sprite->setAnchorPoint(CCPoint(0.5f,0.5f));
        m_sprite->setBlendFunc({GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA});
        m_sprite->setPosition(CCPoint(m_winSize.width*0.5f,m_winSize.height*0.5f));
        m_draw = CCDrawNode::create();
        m_draw->retain();
        addChild(m_sprite);
        return true;
    }
    ~DarknessNode() { CC_SAFE_RELEASE(m_rt); CC_SAFE_RELEASE(m_draw); }

    void redraw(CCNode* parent, PlayerObject* p1, PlayerObject* p2) {
        auto toScreen = [&](PlayerObject* p) {
            return parent->convertToWorldSpace(p->getParent()->convertToWorldSpace(p->getPosition()));
        };
        m_draw->clear();
        drawCone(toScreen(p1), p1->isFlipX());
        if (p2) drawCone(toScreen(p2), p2->isFlipX());
        m_rt->beginWithClear(0,0,0,0);
        m_draw->visit();
        m_rt->end();
        m_sprite->setOpacity((GLubyte)(Mod::get()->getSettingValue<double>("darkness")*255));
    }
};

class $modify(BoobaPlayLayer, PlayLayer) {
    struct Fields { DarknessNode* darkness=nullptr; PixelTrail* trail1=nullptr; PixelTrail* trail2=nullptr; };

    bool init(GJGameLevel* level, bool p1, bool p2) {
        if (!PlayLayer::init(level, p1, p2)) return false;
        int pz = m_player1->getZOrder();
        m_fields->darkness = DarknessNode::create();
        addChild(m_fields->darkness, pz-1);
        m_fields->trail1 = PixelTrail::create();
        addChild(m_fields->trail1, pz+1);
        m_fields->trail2 = PixelTrail::create();
        addChild(m_fields->trail2, pz+1);
        schedule(schedule_selector(BoobaPlayLayer::tick));
        return true;
    }

    void updateTrail(PixelTrail* trail, PlayerObject* p) {
        CCPoint lp = convertToNodeSpace(p->getParent()->convertToWorldSpace(p->getPosition()));
        trail->setPosition(CCPoint(lp.x+(p->isFlipX()?14.f:-14.f),lp.y));
        trail->setVisible(p->m_isOnGround);
    }

    void tick(float) {
        if (!m_player1 || !m_fields->darkness) return;
        bool dual = m_player2 && m_gameState.m_isDualMode;
        m_fields->darkness->redraw(this, m_player1, dual ? m_player2 : nullptr);
        updateTrail(m_fields->trail1, m_player1);
        m_fields->trail2->setVisible(false);
        if (dual) updateTrail(m_fields->trail2, m_player2);
    }
};
