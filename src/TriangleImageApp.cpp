#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/cairo/Cairo.h"

#include "glm/gtc/random.hpp"

#define DO_GL 1

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

vector<Colorf> cols = {

    Colorf(0.000000, 0.000000, 0.000000),
    Colorf(0.000000, 0.160000, 0.360000),
    Colorf(0.000000, 0.270000, 0.520000),
    Colorf(0.000000, 0.360000, 0.670000),
    Colorf(0.000000, 0.480000, 0.760000),
    Colorf(0.000000, 0.600000, 0.850000),
    Colorf(0.740000, 0.740000, 0.740000),
    Colorf(0.840000, 0.840000, 0.840000),
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
            vec2 c(sideLength * 0.5, sideLength * 0.5f * std::sqrt(3.0f));
            
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
            
            if (orientation == UP) {
                output->transform(glm::rotate(mat3(1.0f), 3.14159f / 3.0f));
            }

            
            vec2 relativeCenter(0.0f);
            for (auto &p : output->getContour(0).getPoints()) {
                relativeCenter+= p;
            }
            relativeCenter/= 3.0f;
            
            
            mat3 transform = glm::translate(mat3(1.0), p);
            
            output->transform(transform);
  
           
            vec2 point(0.0f);
            for (auto &p : output->getContour(0).getPoints()) {
                point+= p;
            }
            output->mCenter = point / 3.0f;

            output->mOrientation = orientation;
            output->mSideLength = sideLength;
            output->mRotation = rotation;
            output->mSide = side;
            
            output->mColour = Color(0, 0, 0);
            
            
            //shrink
//            output->transform(glm::translate(mat3(1.0), -output->mCenter));
//            output->transform(glm::scale(mat3(1.0), vec2(0.85, 0.85)));
//            output->transform(glm::translate(mat3(1.0), output->mCenter));

            
            return output;
        }
        
        vector<Colorf> getColors() override {
            return vector<Colorf>(3, mColour);
        }

        
        char mSide;
        int mRotation;
        int mColourIndex;

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
        
        vec2 q(0.0);
        for (auto half : output->mHalves) {
            q+= half->mCenter;
        }
        
        output->mCenter = q / float(output->mHalves.size());
        
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

            float v = pixelSum / 255.0f / nPixels;
            
            
            float d = cols.size();
            v = std::floor(v * d) / d;


            half->mColour = Color(v, v, v);

            int index = size_t(std::floor(v * d));

            if (glm::distance(vec2(413, 366), this->mCenter) > 130 &&
                glm::distance(vec2(737,370), this->mCenter) > 130) {

                int rmax = 6;
                auto r = glm::linearRand(0, rmax);

                if (r == 0) {
                    if (index == 0) index++;
                    else if (index == 1) index--;
                    else index-= 2;
                    
                }
                else if (r == rmax) {
                    if (index + 1 == cols.size() - 1) index+= 1;
                    else if (index == cols.size() - 1) index--;
                    else index+= 2;
                }
                else if (r == 1) {
                    if (index == 0) index++;
                    else index--;
                    
                }
                else if (r == rmax - 1) {
                    if (index == cols.size() - 1) index--;
                    else index++;
                }
            }
            
            half->mColour = cols[index];
            half->mColourIndex = index;

        }
        
    }
    
    vector<Colorf> getColors() override {
        vector<Colorf> output;
        if (!mMerged) {
            for (auto half : mHalves) {
                auto halfColours = half->getColors();
                output.insert(output.end(), halfColours.begin(), halfColours.end());
            }
        }
        else {
            output = vector<Colorf>(3, mColour);
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
    bool mMerged;
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
        
        mTriangles.clear();
        
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
        
        map<std::pair<int, char>, size_t> colourAmounts;
        
        
        for (auto triangle : mTriangles) {
            for (auto half : triangle->mHalves) {
                auto index = make_pair(half->mColourIndex, half->mSide);
                
                if (colourAmounts.count(index) == 0) {
                    colourAmounts.emplace(index, 0);
                }
                
                ++colourAmounts[index];
            }
        }
        
        cout << "colour amounts" << endl;
        size_t total = 0;
        for (auto &pair : colourAmounts) {
            cout << "Colour " << (pair.first.first + 1) << "-" << pair.first.second << ": " << pair.second << endl;
            total+= pair.second;
        }
        cout << "total = " << total << endl;
        cout << endl;

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




void TriangleImageApp::setup() {

    
#ifdef DO_GL
    mChannel = Channel::create(loadImage(loadAsset("giulia_marco.jpg")));
    mTexture = gl::Texture::create(*mChannel.get());
#else
    mChannel = Channel::create(loadImage(loadAsset("giulia_marco_flip.jpg")));
#endif

    setWindowSize(mChannel->getSize());
    setWindowPos(10, 10);
    
    float start = getElapsedSeconds();
    
    mTriGrid.setup(getWindowSize(), 24);
    mTriGrid.colourTriangles(*mChannel.get());
    mTriGrid.populateMesh();
    
    cout << (getElapsedSeconds() - start) << endl;
    
    cout << (mTriGrid.mTriangles.size() * 6) << endl;

    cout << (mTriGrid.mNumTris * 2) << endl;
    
    cout << mTriGrid.mSideLength << endl;
    
    }

void TriangleImageApp::mouseDown( MouseEvent event )
{
    mTriGrid.setup(getWindowSize(), 24);

    mTriGrid.colourTriangles(*mChannel.get());
    mTriGrid.populateMesh();
}

void TriangleImageApp::update()
{
}

void TriangleImageApp::draw()
{

#ifdef DO_GL
	gl::clear( Color( 1, 1, 1 ) );

    gl::color(1, 1, 1);
    gl::disableWireframe();


    gl::pushMatrices();
    
    gl::draw(*mTriGrid.mTriMesh.get());
    

//    auto &cols = mTriGrid.mTriMesh->getBufferColors();
//    vector<float> colsCopy(cols.begin(), cols.end());
//    cols.clear();
//    for (size_t i = 0; i < colsCopy.size(); ++i) {
//        cols.push_back(0.1f);
//    }
//    gl::enableWireframe();
//
//    gl::draw(*mTriGrid.mTriMesh.get());
//    
//    cols = colsCopy;
//
//    
//    gl::disableWireframe();
////
//    gl::color(1, 0, 0);
//    for (auto ht : mTriGrid.mTriangles) {
//        for (auto half : ht->mHalves) {
//            gl::drawSolidCircle(half->mCenter, 2);
////            gl::drawString(to_string(half->mColourIndex), half->mCenter, ColorA(0, 0, 0, 1));
//        }
//    }
//
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
    
    
#else
    
    
    string bit = "e";
    
    auto path = getHomeDirectory() / "Desktop" / "tris-cut-col.svg";
    auto surf = cairo::SurfaceSvg(path, getWindowWidth(), getWindowHeight() );
    cairo::Context ctx( surf );

    ctx.setFontSize(7);
    ctx.setLineWidth(0.1);
    for (auto triangle : mTriGrid.mTriangles) {
        for (auto half : triangle->mHalves) {
            ctx.setSource(Color(0.1, 0.1, 0.1));
            ctx.appendPath(*half.get());
            ctx.stroke();
            ctx.setSource(cols[half->mColourIndex]);
            ctx.appendPath(*half.get());
            ctx.fill();
        }
    }
    
    for (auto triangle : mTriGrid.mTriangles) {
        for (auto half : triangle->mHalves) {
            ctx.moveTo(half->mCenter - vec2(2.0, -2.0));
            ctx.showText(toString(half->mColourIndex));
        }
    }

    
    ctx.save();
    quit();

#endif
    
    
}

#ifdef DO_GL
CINDER_APP( TriangleImageApp, RendererGl )
#else
CINDER_APP( TriangleImageApp, Renderer2d )
#endif

