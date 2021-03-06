#ifndef TRACEBASE_HPP_
#define TRACEBASE_HPP_

#include "TraceSettings.hpp"

#include "samplerecords/SurfaceScatterEvent.hpp"
#include "samplerecords/VolumeScatterEvent.hpp"
#include "samplerecords/LightSample.hpp"

#include "sampling/SampleGenerator.hpp"
#include "sampling/UniformSampler.hpp"
#include "sampling/SampleWarp.hpp"

#include "renderer/TraceableScene.hpp"

#include "cameras/Camera.hpp"

#include "volume/Medium.hpp"

#include "math/TangentFrame.hpp"
#include "math/MathUtil.hpp"
#include "math/Angle.hpp"

#include "bsdfs/Bsdf.hpp"

#include <vector>
#include <memory>
#include <cmath>

namespace Tungsten {

class TraceBase
{
    static CONSTEXPR bool GeneralizedShadowRays = true;

protected:
    const TraceableScene *_scene;
    TraceSettings _settings;
    uint32 _threadId;

    std::vector<float> _lightPdf;

    TraceBase(TraceableScene *scene, const TraceSettings &settings, uint32 threadId);

    SurfaceScatterEvent makeLocalScatterEvent(IntersectionTemporary &data, IntersectionInfo &info,
            Ray &ray, SampleGenerator *sampler, UniformSampler *supplementalSampler) const;

    bool isConsistent(const SurfaceScatterEvent &event, const Vec3f &w) const;

    Vec3f generalizedShadowRay(Ray &ray,
                               const Medium *medium,
                               const Primitive *endCap,
                               int bounce);

    Vec3f attenuatedEmission(const Primitive &light,
                             const Medium *medium,
                             float expectedDist,
                             IntersectionTemporary &data,
                             IntersectionInfo &info,
                             int bounce,
                             Ray &ray);

    Vec3f lightSample(const TangentFrame &frame,
                      const Primitive &light,
                      const Bsdf &bsdf,
                      SurfaceScatterEvent &event,
                      const Medium *medium,
                      int bounce,
                      float epsilon,
                      const Ray &parentRay);

    Vec3f bsdfSample(const TangentFrame &frame,
                         const Primitive &light,
                         const Bsdf &bsdf,
                         SurfaceScatterEvent &event,
                         const Medium *medium,
                         int bounce,
                         float epsilon,
                         const Ray &parentRay);

    Vec3f volumeLightSample(VolumeScatterEvent &event,
                        const Primitive &light,
                        const Medium *medium,
                        bool performMis,
                        int bounce,
                        const Ray &parentRay);

    Vec3f volumePhaseSample(const Primitive &light,
                        VolumeScatterEvent &event,
                        const Medium *medium,
                        int bounce,
                        const Ray &parentRay);

    Vec3f sampleDirect(const TangentFrame &frame,
                       const Primitive &light,
                       const Bsdf &bsdf,
                       SurfaceScatterEvent &event,
                       const Medium *medium,
                       int bounce,
                       float epsilon,
                       const Ray &parentRay);

    Vec3f volumeSampleDirect(const Primitive &light,
                        VolumeScatterEvent &event,
                        const Medium *medium,
                        int bounce,
                        const Ray &parentRay);

    const Primitive *chooseLight(SampleGenerator &sampler, const Vec3f &p, float &weight);

    Vec3f volumeEstimateDirect(VolumeScatterEvent &event,
                        const Medium *medium,
                        int bounce,
                        const Ray &parentRay);

    Vec3f estimateDirect(const TangentFrame &frame,
                         const Bsdf &bsdf,
                         SurfaceScatterEvent &event,
                         const Medium *medium,
                         int bounce,
                         float epsilon,
                         const Ray &parentRay);

    bool handleVolume(SampleGenerator &sampler, UniformSampler &supplementalSampler,
               const Medium *&medium, int bounce, bool handleLights, Ray &ray,
               Vec3f &throughput, Vec3f &emission, bool &wasSpecular, bool &hitSurface,
               Medium::MediumState &state);

    bool handleSurface(IntersectionTemporary &data, IntersectionInfo &info,
                       SampleGenerator &sampler, UniformSampler &supplementalSampler,
                       const Medium *&medium, int bounce, bool handleLights, Ray &ray,
                       Vec3f &throughput, Vec3f &emission, bool &wasSpecular,
                       Medium::MediumState &state);
};

}

#endif /* TRACEBASE_HPP_ */
