#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

using namespace ci;
using namespace ci::app;
using namespace std;

using TriangleRef = shared_ptr<class Triangle>;

class Triangle : public Path2d {
public:

    enum Orientation { UP, DOWN };
    
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
        
        output->mCenter = output->calcBoundingBox().getCenter();
        output->mOrientation = down ? DOWN : UP;
        output->mSideLength = sideLength;
//        output->mOffset = p;

        output->mColour = Color(p.x / 600.0f, p.y / 400.0f, 0.5);

        return output;
    }
    
    void setColourFromChannel(Channel &channel) {
        // get the cache
        // loop through cache
        // which is a 2d array of bools
        // or a list of coords - that need to be offset for the triangle
        // go though region of channel that corresponds to
        shared_ptr<PixelCache> pixels;
        try {
            pixels = pixelCache.at(make_pair(mSideLength, mOrientation));
        }
        catch (std::out_of_range e) {
            addToPixelCache(mSideLength);
            pixels = pixelCache.at(make_pair(mSideLength, mOrientation));
        }
        
        auto *channelData = channel.getData();
        
//        auto channelAreaIter = channel.getIter(pixels->mBoundingBox);
        uint32_t pixelSum = 0;
        float nPixels = 0;
        for (auto &offset : pixels->mInsidePositions) {

            ivec2 pos = offset + ivec2(mCenter.x, mCenter.y);
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
    
    vec2 getCenter() { return mCenter; }
    ivec2 getICenter() { return ivec2(mCenter.x, mCenter.y); }
//    vec2 mOffset;
    
    
//    using PixelCacheRef = shared_ptr<struct Triangle::PixelCache>;
    
    struct PixelCache {
        float mSize;
        vec2 mCenter;
        vector<ivec2> mInsidePositions;
        Area mBoundingBox;
        
        static uint boxRadius(float sideLength) {
            return static_cast<uint>(std::ceil(sideLength / sqrt(3.0f)));
        }
        
        static shared_ptr<PixelCache> createWithOrientation(float sideLength, Orientation orientation) {
            
            auto downTriangle = Triangle::create(vec2(0.0f), sideLength, orientation == DOWN);
            
            auto r = boxRadius(sideLength);
            auto center = downTriangle->getCenter();
            Area boundingBox(center - vec2(r, r), center + vec2(r, r));
            
            shared_ptr<PixelCache> output = make_shared<PixelCache>();
            
            for (float x = boundingBox.getX1(); x < boundingBox.getX2(); x++) {
                for (float y = boundingBox.getY1(); y < boundingBox.getY2(); y++) {
                    if (downTriangle->contains(vec2(x, y))) {
                        output->mInsidePositions.push_back(ivec2(x, y));
                    }
                }
            }
            
            output->mBoundingBox = boundingBox;
            
            return output;
        }
    };
    
    static void addToPixelCache(float sideLength) {
        pixelCache.emplace(make_pair(sideLength, DOWN), PixelCache::createWithOrientation(sideLength, DOWN));
        pixelCache.emplace(make_pair(sideLength, UP), PixelCache::createWithOrientation(sideLength, UP));
    }
    
    static map<pair<float, Orientation>, shared_ptr<PixelCache>> pixelCache;

    Orientation mOrientation;
    vec2 mCenter;
    float mSideLength;
    Color mColour;
};

map<pair<float, Triangle::Orientation>, shared_ptr<Triangle::PixelCache>> Triangle::pixelCache;

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
            mTriMesh->appendPositions(&triangle->getPoints()[0], 3);
            for (size_t i = 0; i < 3; i++) {
                mTriMesh->appendColorRgb(triangle->mColour);
            }
            
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
    
    auto tri = Triangle::create(vec2(0.0f), 12);
    auto pc = Triangle::PixelCache::createWithOrientation(12, Triangle::UP);
    map<pair<int, int>, bool> points;
    for (auto p : pc->mInsidePositions) {
        points.emplace(make_pair(int(p.x), int(p.y)), true);
//        cout << (vec2(p.x, p.y) +tri->getCenter()) << endl;
    }
    
    for (int y = pc->mBoundingBox.getY1(); y < pc->mBoundingBox.getY2(); y++) {
        for (int x = pc->mBoundingBox.getX1(); x < pc->mBoundingBox.getX2(); x++) {
            if (points.count(make_pair(x, y)) > 0) {
                cout << "1";
            }
            else {
                cout << "0";
            }
        }
        cout << endl;
    }
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
//    gl::draw(mTexture);

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
