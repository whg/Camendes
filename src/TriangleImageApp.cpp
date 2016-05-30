#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

using namespace ci;
using namespace ci::app;
using namespace std;

//using BaseTileRef = std::shared_ptr<BaseTile>;

class BaseTile : public Shape2d {
public:
    virtual void setColourFromChannel(Channel &channel) = 0;
    virtual vector<Colorf> getColors() = 0;
    virtual string getId() = 0;
    virtual vec2 getCenter() const { return mCenter; }
    
protected:
    vec2 mCenter;
};

struct PixelCache {

    vector<ivec2> mInsidePositions;
    Area mBoundingBox;
    
    static shared_ptr<PixelCache> create(BaseTile &tile, uint radius) {

        auto center = tile.getCenter();
        Area boundingBox(center - vec2(radius, radius), center + vec2(radius, radius));

        shared_ptr<PixelCache> output = make_shared<PixelCache>();
        
        for (float x = boundingBox.getX1(); x < boundingBox.getX2(); x++) {
            for (float y = boundingBox.getY1(); y < boundingBox.getY2(); y++) {
                if (tile.contains(vec2(x, y))) {
                    output->mInsidePositions.push_back(ivec2(x, y));
                }
            }
        }
        
        output->mBoundingBox = boundingBox;
        
        return output;
    }

    static map<string, shared_ptr<PixelCache>> pixelCache;

};


map<string, shared_ptr<PixelCache>> PixelCache::pixelCache;



using TriangleRef = shared_ptr<class Triangle>;



class Triangle : public BaseTile {
public:

    enum Orientation { UP, DOWN };
    
    string getId() override {
        char id[50];
        sprintf(id, "ETri-%2.2f-%s", mSideLength, mOrientation == UP ? "u" : "d");
        return string(id);
    }
    
    static TriangleRef create(vec2 p, float sideLength, bool down=true) {

        auto output = make_shared<Triangle>();
        
        output->moveTo(vec2(0));
        output->lineTo(vec2(sideLength, 0));
        output->lineTo(vec2(sideLength * 0.5, sideLength * 0.5f * sqrt(3.0f)));
        output->close();
        
        mat3 transform = glm::translate(mat3(1.0), p);
        
        if (!down) {
            transform = glm::rotate(transform, 3.14159f / 3.0f);
        }
        
        output->transform(transform);
        
//        output->mCenter = output->calcBoundingBox().getCenter();
        vec2 point(0.0f);
        for (auto &p : output->getContour(0).getPoints()) {
            point+= p;
        }
        output->mCenter = point / 3.0f;
        output->mOrientation = down ? DOWN : UP;
        output->mSideLength = sideLength;

        output->mColour = Color(p.x / 600.0f, p.y / 400.0f, 0.5);

        return output;
    }
    
    void setColourFromChannel(Channel &channel) override {
        shared_ptr<PixelCache> pixels;

        try {
            pixels = PixelCache::pixelCache.at(getId());
        }
        catch (std::out_of_range e) {
//            addToPixelCache(mSideLength);
            float radius = mSideLength / std::sqrt(3.0f);
            auto up = create(vec2(0), mSideLength, true);
            PixelCache::pixelCache.emplace(up->getId(), PixelCache::create(*up.get(), radius));
            auto down = create(vec2(0), mSideLength, false);
            PixelCache::pixelCache.emplace(down->getId(), PixelCache::create(*down.get(), radius));
//            pixelCache.emplace(make_pair(sideLength, DOWN), PixelCache::createWithOrientation(sideLength, DOWN));
//            pixelCache.emplace(make_pair(sideLength, UP), PixelCache::createWithOrientation(sideLength, UP));

            pixels = PixelCache::pixelCache.at(getId());
        }
        
        auto *channelData = channel.getData();
        
//        auto channelAreaIter = channel.getIter(pixels->mBoundingBox);
        uint32_t pixelSum = 0;
        float nPixels = 0;
        for (auto &offset : pixels->mInsidePositions) {

            ivec2 pos = ivec2(mCenter.x, mCenter.y) - offset;
            if (channel.getBounds().contains(pos)) {
//            cout << pos << endl;
//                cout << channelData[pos.x + pos.y * channel.getRowBytes()] << endl;
//                cout << endl;
                pixelSum += uint32_t(channelData[pos.x + pos.y * channel.getRowBytes()]);
                nPixels++;
            }
            
        }
        
        float v = pixelSum / 255.0f / nPixels;
        float d = 6;
        v = std::floor(v * d) / d;
//        if (v > 0.25) v = 1.0;
//        else v = 0.0;
        
//        cout << pixelSum  << endl;
//        cout <<pixels->mInsidePositions.size() << endl;
//        cout << v << endl;
//        cout << n << endl;
//        cout << endl;
//        v = 1.0f;
        mColour = Color(v, v, v);
    }
    
    vector<Colorf> getColors() override {
        return vector<Colorf>(3, mColour);
    }
    
    vec2 getCenter() const override { return mCenter; }
    ivec2 getICenter() { return ivec2(mCenter.x, mCenter.y); }
//    vec2 mOffset;
    
    
//    using PixelCacheRef = shared_ptr<struct Triangle::PixelCache>;
    
    
    Orientation mOrientation;
    vec2 mCenter;
    float mSideLength;
    Color mColour;
};


class TriGrid {
public:
    TriGrid() {}
    
    void setup(ivec2 size, size_t numTrianglesAcross) {
        mSideLength = static_cast<float>(size.x) / numTrianglesAcross;
        float triangleHeight = mSideLength * 0.5f * sqrt(3.0f);
        
        mNumTris.x = std::floor(size.x / mSideLength);
        mNumTris.y = std::floor(size.y / triangleHeight);
        
        
        mTriMesh = TriMesh::create(TriMesh::Format().positions(2).colors(3));
        
        
        for (size_t y = 0; y < mNumTris.y; y++) {
        
            for (size_t i = 0; i < mNumTris.x; i++) {
                vec2 p(i * mSideLength, y * triangleHeight);
                
                if ((y + 1) % 2 == 1) {
                    p.x+= mSideLength * 0.5;
                }
                
                bool doDown = (y) % 2;
                auto downTri = Triangle::create(p, mSideLength, doDown);
                mTriangles.push_back(downTri);
                
                if ((y % 2 == 0 && i != mNumTris.x - 1) || (y % 2 == 1 && i != 0)) {
                    auto upTri = Triangle::create(p, mSideLength, !doDown);
                    mTriangles.push_back(upTri);
                }

            }
        }
        
        populateMesh();
    }
    
    void populateMesh() {
        mTriMesh->clear();
        for (auto triangle : mTriangles) {
            for (auto &path : triangle->getContours()) {
                mTriMesh->appendPositions(&path.getPoints()[0], path.getPoints().size());
            }
            mTriMesh->appendColors(&triangle->getColors()[0], triangle->getColors().size());
        }
    }
    
    void colourTriangles(Channel &channel) {
        for (auto triangle : mTriangles) {
            triangle->setColourFromChannel(channel);
        }
    }
    
    
    TriMeshRef mTriMesh;
    float mSideLength;
    ivec2 mNumTris;
    
    vector<TriangleRef> mTriangles;
};

class TriangleImageApp : public App {
  public:
	void setup() override;
	void mouseDown( MouseEvent event ) override;
	void update() override;
	void draw() override;
    
    TriMeshRef mTriMesh;
    
    TriGrid mTriGrid;
    
    SurfaceRef mSurface;
    ChannelRef mChannel;
    gl::TextureRef mTexture;
};




void TriangleImageApp::setup()
{
    
//    auto tri = Triangle::create(vec2(0.0f), 12);
//    auto pc = Triangle::PixelCache::createWithOrientation(12, Triangle::UP);
//    map<pair<int, int>, bool> points;
//    for (auto p : pc->mInsidePositions) {
//        points.emplace(make_pair(int(p.x), int(p.y)), true);
////        cout << (vec2(p.x, p.y) +tri->getCenter()) << endl;
//    }
//    
//    for (int y = pc->mBoundingBox.getY1(); y < pc->mBoundingBox.getY2(); y++) {
//        for (int x = pc->mBoundingBox.getX1(); x < pc->mBoundingBox.getX2(); x++) {
//            if (points.count(make_pair(x, y)) > 0) {
//                cout << "1";
//            }
//            else {
//                cout << "0";
//            }
//        }
//        cout << endl;
//    }
//    for (auto &p : pc->mInsidePositions) cout << p;
//    cout << endl;
//    quit();
    
    
//    mSurface = Surface::create(loadAsset("cat.png"));
//    Surface surface(loadAsset("cat.png"))
    mChannel = Channel::create(loadImage(loadAsset("couple.jpg")));
    mTexture = gl::Texture::create(*mChannel.get());
    setWindowSize(mChannel->getSize());
    
    float start = getElapsedSeconds();
    
    mTriGrid.setup(getWindowSize(), 50);
    mTriGrid.colourTriangles(*mChannel.get());
    mTriGrid.populateMesh();
    
    cout << (getElapsedSeconds() - start) << endl;

}

void TriangleImageApp::mouseDown( MouseEvent event )
{
}

void TriangleImageApp::update()
{
}

void TriangleImageApp::draw()
{
	gl::clear( Color( 1, 1, 1 ) );
//    gl::color(0, 0, 0);
//
    gl::disableWireframe();
    gl::draw(mTexture);

    gl::pushMatrices();
//    gl::translate(vec2(20, 20));
    gl::enableWireframe();
    gl::draw(*mTriGrid.mTriMesh.get());
    
    gl::popMatrices();
    
    


    
//    gl::draw
    
//    auto tri = makeTriangle(vec2(getMousePos() - getWindowPos()), 50);
//    gl::draw(tri);
}

CINDER_APP( TriangleImageApp, RendererGl )
