#ifndef BLADETEXTURE_HPP_
#define BLADETEXTURE_HPP_

#include "Texture.hpp"

namespace Tungsten {

class BladeTexture : public Texture
{
    typedef JsonSerializable::Allocator Allocator;

    int _numBlades;
    float _angle;

    float _area;
    float _bladeAngle;
    Vec2f _baseNormal;
    Vec2f _baseEdge;

    void init();

public:
    BladeTexture();

    virtual void fromJson(const rapidjson::Value &v, const Scene &scene) override;
    virtual rapidjson::Value toJson(Allocator &allocator) const override;

    virtual bool isConstant() const override;

    virtual Vec3f average() const override;
    virtual Vec3f minimum() const override;
    virtual Vec3f maximum() const override;

    virtual Vec3f operator[](const Vec2f &uv) const override final;
    virtual Vec3f operator[](const IntersectionInfo &info) const override;
    virtual void derivatives(const Vec2f &uv, Vec2f &derivs) const override;

    virtual void makeSamplable(TextureMapJacobian jacobian) override;
    virtual Vec2f sample(TextureMapJacobian jacobian, const Vec2f &uv) const override;
    virtual float pdf(TextureMapJacobian jacobian, const Vec2f &uv) const override;
};

}

#endif /* BLADETEXTURE_HPP_ */
