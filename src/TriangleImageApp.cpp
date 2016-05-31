#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

#include "glm/gtc/random.hpp"

using namespace ci;
using namespace ci::app;
using namespace std;

class BaseTile : public Shape2d {
public:
    virtual void setColourFromChannel(Channel &channel) = 0;
    virtual vector<Colorf> getColors() = 0;
    virtual string getId() = 0;
    virtual vec2 getCenter() const { return mCenter; }
    
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
            float radius = mSideLength / std::sqrt(3.0f);
            auto up = create(vec2(0), mSideLength, true);
            PixelCache::pixelCache.emplace(up->getId(), PixelCache::create(*up.get(), radius));
            auto down = create(vec2(0), mSideLength, false);
            PixelCache::pixelCache.emplace(down->getId(), PixelCache::create(*down.get(), radius));

            pixels = PixelCache::pixelCache.at(getId());
        }
        
        auto *channelData = channel.getData();
        
        uint32_t pixelSum = 0;
        float nPixels = 0;
        for (auto &offset : pixels->mInsidePositions) {

            ivec2 pos = ivec2(mCenter.x, mCenter.y) - offset;
            if (channel.getBounds().contains(pos)) {
                pixelSum += uint32_t(channelData[pos.x + pos.y * channel.getRowBytes()]);
                nPixels++;
            }
            
        }
        
        float v = pixelSum / 255.0f / nPixels;
        float d = 6;
        v = std::floor(v * d) / d;

        mColour = Color(v, v, v);
    }
    
    vector<Colorf> getColors() override {
        return vector<Colorf>(3, mColour);
    }
    
    vec2 getCenter() const override { return mCenter; }
    ivec2 getICenter() { return ivec2(mCenter.x, mCenter.y); }
    
    Orientation mOrientation;
    float mSideLength;
    Color mColour;
};

class HalvedTriangles : public Triangle {
public:
    
    struct HalfTriangle : public Triangle {
        string getId() override {
            char id[50];
            sprintf(id, "HTri-%2.2f-%s-%c-%d", mSideLength, mOrientation == UP ? "u" : "d", mSide, mRotation);
            return string(id);
        }
        
        static shared_ptr<HalfTriangle> create(vec2 p, float sideLength, Orientation orientation, int rotation, char side) {
            
            auto output = make_shared<HalfTriangle>();
            
            vec2 center(sideLength * 0.5, sideLength / std::sqrt(3.0f) / 2.0f);
            vec2 a(0.0f);
            vec2 b(sideLength, 0);
            vec2 c(sideLength * 0.5, sideLength * 0.5f * sqrt(3.0f));
            
            if (rotation == 120 && side == 'r') {
                output->moveTo(a);
                output->lineTo(center);
                output->lineTo((a + c) * 0.5f);
                output->close();
            }
            else if (rotation == 120 && side == 'l') {
                output->moveTo(a);
                output->lineTo((a + b) * 0.5f);
                output->lineTo(center);
                output->close();
            }
            else if (rotation == 60 && side == 'r') {
                output->moveTo(b);
                output->lineTo(center);
                output->lineTo((b + a) * 0.5f);
                output->close();
            }
            else if (rotation == 60 && side == 'l') {
                output->moveTo(b);
                output->lineTo((b + c) * 0.5f);
                output->lineTo(center);
                output->close();
            }
            else if (rotation == 0 && side == 'r') {
                output->moveTo(c);
                output->lineTo(center);
                output->lineTo((c + b) * 0.5f);
                output->close();
            }
            else if (rotation == 0 && side == 'l') {
                output->moveTo(c);
                output->lineTo((c + a) * 0.5f);
                output->lineTo(center);
                output->close();
            }
            else {
                assert(1 == 0);
            }
            
            mat3 transform = glm::translate(mat3(1.0), p);
            
            if (orientation == UP) {
                transform = glm::rotate(transform, 3.14159f / 3.0f);
                
            }
            
            output->transform(transform);
            
//            vec2 point(0.0f);
//            for (auto &p : output->getContour(0).getPoints()) {
//                point+= p;
//            }
//            output->mCenter = point / 3.0f;
            output->mCenter = (a + b + c) / 3.0f;
            if (orientation == UP) {
                output->mCenter = glm::rotate(output->mCenter, 3.14159f / 3.0f);
            }
            output->mCenter+= p;
            
            output->mOrientation = orientation;
            output->mSideLength = sideLength;
            output->mRotation = rotation;
            output->mSide = side;
            
            //        output->mColour = Color(p.x / 600.0f, p.y / 400.0f, 0.5);
            output->mColour = Color(0, 0, 0);
            
            return output;
        }
        
        vector<Colorf> getColors() override {
            return vector<Colorf>(3, mColour);
        }

        
        char mSide;
        int mRotation;

    };
    
    string getId() override {
        char id[50];
        sprintf(id, "HTris-%2.2f", mSideLength);
        return string(id);
    }
    
    static shared_ptr<HalvedTriangles> create(vec2 p, float sideLength, bool down=true) {
        
        auto output = make_shared<HalvedTriangles>();
        
        vector<int> angles = { 0, 60, 120 };
        vector<char> sides = { 'l', 'r' };
        for (auto angle : angles) {
            for (auto side : sides) {
                auto hf = HalfTriangle::create(p, sideLength, down ? DOWN : UP, angle, side);
                output->append(*hf.get());
                output->mHalves.push_back(hf);
            }
        }
        
        
        output->mOrientation = down ? DOWN : UP;
        output->mSideLength = sideLength;
        output->mOffset = p;
        output->mColour = Color(0, 1, 0);
        
        return output;
    }
    
    void setColourFromChannel(Channel &channel) override {
        shared_ptr<PixelCache> pixels;

        mColours.clear();
        
        for (auto half : mHalves) {
        
            try {
                pixels = PixelCache::pixelCache.at(half->getId());
            }
            catch (std::out_of_range e) {
                float radius = mSideLength / 2.0f;
                auto freshHalf = HalfTriangle::create(vec2(0), half->mSideLength, half->mOrientation, half->mRotation, half->mSide);
                
                PixelCache::pixelCache.emplace(freshHalf->getId(), PixelCache::create(*freshHalf.get(), radius));
                
                assert(freshHalf->getId() == half->getId());
                
                pixels = PixelCache::pixelCache.at(half->getId());
            }
            
            auto *channelData = channel.getData();
            
            uint32_t pixelSum = 0;
            float nPixels = 0;
            for (auto &offset : pixels->mInsidePositions) {
                
                ivec2 pos = ivec2(mOffset.x, mOffset.y) + offset;
                if (channel.getBounds().contains(pos)) {
                    pixelSum += uint32_t(channelData[pos.x + pos.y * channel.getRowBytes()]);
                    nPixels++;
                }
                
            }
            
            vector<Colorf> cols = {
                Colorf(0.074510, 0.094118, 0.117647),
                Colorf(0.109804, 0.113725, 0.137255),
                Colorf(0.129412, 0.152941, 0.298039),
                Colorf(0.109804, 0.152941, 0.372549),
                Colorf(0.113725, 0.227451, 0.494118),
                Colorf(0.117647, 0.337255, 0.615686),
                Colorf(0.478431, 0.525490, 0.580392),
                Colorf(0.619608, 0.643137, 0.686275),
                Colorf(0.843137, 0.870588, 0.909804),
            };
            
            float v = pixelSum / 255.0f / nPixels;
            float d = cols.size();
//            v = std::floor(v * d) / d;
//            v+= 0.125;

//            int rmax = 6;
//            auto r = glm::linearRand(0, rmax);
//            if (r == 0) {
//                v-= 1.0f / d;
//            }
//            else if (r == rmax) {
//                v+= 1.0f / d;
//            }

//            half->mColour = Color(v, v, v);
            
            int index = size_t(std::floor(v * d));

            int rmax = 6;
            auto r = glm::linearRand(0, rmax);
            if (r == 0) {
                index--;
            }
            else if (r == rmax) {
                index++;
            }
            
            index = std::min(int(cols.size() - 1), std::max(0, index));

            if (index < 0 || index >= cols.size()) {
                cout << "asdf" << endl;
            }

            half->mColour = cols[index];

        }
        
    }
    
    vector<Colorf> getColors() override {
        vector<Colorf> output;
        for (auto half : mHalves) {
            auto halfColours = half->getColors();
            output.insert(output.end(), halfColours.begin(), halfColours.end());
        }
        return output;
    }
    
    vec2 getCenter() const override { return mCenter; }
    ivec2 getICenter() { return ivec2(mCenter.x, mCenter.y); }
    
    Orientation mOrientation;
    float mSideLength;
    Color mColour;
    vector<Colorf> mColours;
    vec2 mOffset;
    
    vector<shared_ptr<HalfTriangle>> mHalves;
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
                auto downTri = HalvedTriangles::create(p, mSideLength, doDown);
                mTriangles.push_back(downTri);
                
                if ((y % 2 == 0 && i != mNumTris.x - 1) || (y % 2 == 1 && i != 0)) {
                    auto upTri = HalvedTriangles::create(p, mSideLength, !doDown);
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
    
    vector<shared_ptr<HalvedTriangles>> mTriangles;
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

//    mChannel = Channel::create(loadImage(loadAsset("couple.jpg")));
    mChannel = Channel::create(loadImage(loadAsset("giulia_marco.jpg")));
    mTexture = gl::Texture::create(*mChannel.get());
    setWindowSize(mChannel->getSize());
    setWindowPos(10, 10);
    
    float start = getElapsedSeconds();
    
    mTriGrid.setup(getWindowSize(), 24);
    mTriGrid.colourTriangles(*mChannel.get());
    mTriGrid.populateMesh();
    
    cout << (getElapsedSeconds() - start) << endl;
    
    cout << (mTriGrid.mTriangles.size() * 6) << endl;


    setWindowSize(ivec2((mTriGrid.mNumTris.x - 1)  * mTriGrid.mSideLength, mTriGrid.mNumTris.y * mTriGrid.mSideLength / std::sqrt(3.0f) * 1.5));
}

void TriangleImageApp::mouseDown( MouseEvent event )
{
    mTriGrid.colourTriangles(*mChannel.get());
    mTriGrid.populateMesh();
}

void TriangleImageApp::update()
{
}

void TriangleImageApp::draw()
{
	gl::clear( Color( 1, 1, 1 ) );

    gl::color(1, 1, 1);
    gl::disableWireframe();
//    gl::draw(mTexture);

    gl::pushMatrices();
    gl::translate(vec2(-mTriGrid.mSideLength * 0.5f, 0.0f));
    
    gl::draw(*mTriGrid.mTriMesh.get());
    

//    auto &cols = mTriGrid.mTriMesh->getBufferColors();
//    vector<float> colsCopy(cols.begin(), cols.end());
//    cols.clear();
//    for (size_t i = 0; i < colsCopy.size(); ++i) {
//        cols.push_back(0.9f);
//    }
//    gl::enableWireframe();
//
//    gl::draw(*mTriGrid.mTriMesh.get());
//    
//    cols = colsCopy;

    
//    
//    gl::color(1, 0, 0);
//    for (auto ht : mTriGrid.mTriangles) {
//        for (auto half : ht->mHalves) {
//            gl::drawSolidCircle(half->mCenter, 2);
//        }
//    }
//
//    gl::color(0, 1, 0);
//    for (auto p :PixelCache::pixelCache.at("HTri-50.00-u-l-0")->mInsidePositions) {
////        auto c = mTriGrid.mTriangles[0]->mHalves[0]->mCenter;
////        auto c = mTriGrid.mTriangles[0]->mOffset;
////        cout << mTriGrid.mTriangles[0]->mHalves[0]->getId() << endl;
////        p = ivec2(c.x, c.y) + p;
//        gl::drawSolidRect(Rectf(p.x, p.y, p.x+1, p.y+1));
//    }
//
//    gl::color(0, 0, 1);
//    gl::drawSolidCircle(mTriGrid.mTriangles[0]->mHalves[0]->mCenter, 2);
//    
    gl::popMatrices();
    
}

CINDER_APP( TriangleImageApp, RendererGl )
