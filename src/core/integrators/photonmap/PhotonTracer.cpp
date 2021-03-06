#include "PhotonTracer.hpp"

namespace Tungsten {

PhotonTracer::PhotonTracer(TraceableScene *scene, const PhotonMapSettings &settings, uint32 threadId)
: TraceBase(scene, settings, threadId),
  _settings(settings),
  _photonQuery(new const Photon *[settings.gatherCount]),
  _distanceQuery(new float[settings.gatherCount])
{
    std::vector<float> lightWeights(scene->lights().size());
    for (size_t i = 0; i < scene->lights().size(); ++i) {
        scene->lights()[i]->makeSamplable(_threadId);
        lightWeights[i] = 1.0f; // TODO
    }
    _lightSampler.reset(new Distribution1D(std::move(lightWeights)));
}

void PhotonTracer::tracePhoton(SurfacePhotonRange &surfaceRange, VolumePhotonRange &volumeRange,
        SampleGenerator &sampler, UniformSampler &supplementalSampler)
{
    float u = supplementalSampler.next1D();
    int lightIdx;
    _lightSampler->warp(u, lightIdx);

    LightSample sample(&sampler);
    if (!_scene->lights()[lightIdx]->sampleOutboundDirection(_threadId, sample))
        return;

    Ray ray(sample.p, sample.d);
    Vec3f throughput(sample.weight/_lightSampler->pdf(lightIdx));

    IntersectionTemporary data;
    IntersectionInfo info;
    Medium::MediumState state;
    state.reset();
    Vec3f emission(0.0f);
    const Medium *medium = sample.medium;

    int bounce = 0;
    bool wasSpecular = true;
    bool hitSurface = true;
    bool didHit = _scene->intersect(ray, data, info);
    while ((didHit || medium) && bounce < _settings.maxBounces - 2) {
        ray.advanceFootprint();

        if (medium) {
            VolumeScatterEvent event(&sampler, &supplementalSampler, throughput, ray.pos(), ray.dir(), ray.farT());
            if (!medium->sampleDistance(event, state))
                break;
            throughput *= event.throughput;
            event.throughput = Vec3f(1.0f);

            if (event.t < event.maxT) {
                event.p += event.t*event.wi;

                if (!volumeRange.full()) {
                    VolumePhoton &p = volumeRange.addPhoton();
                    p.pos = event.p;
                    p.dir = ray.dir();
                    p.power = throughput;
                }

                if (medium->absorb(event, state))
                    break;
                if (!medium->scatter(event))
                    break;
                ray = ray.scatter(event.p, event.wo, 0.0f, event.pdf);
                ray.setPrimaryRay(false);
                throughput *= event.throughput;
                hitSurface = false;
            } else {
                hitSurface = true;
            }
        }

        if (hitSurface) {
            if (!info.bsdf->lobes().isPureSpecular() && !surfaceRange.full()) {
                Photon &p = surfaceRange.addPhoton();
                p.pos = info.p;
                p.dir = ray.dir();
                p.power = throughput;
            }
        }

        if (volumeRange.full() && surfaceRange.full())
            break;

        if (hitSurface && !handleSurface(data, info, sampler, supplementalSampler, medium, bounce,
                false, ray, throughput, emission, wasSpecular, state))
            break;

        if (throughput.max() == 0.0f)
            break;

        if (std::isnan(ray.dir().sum() + ray.pos().sum()))
            break;
        if (std::isnan(throughput.sum()))
            break;

        bounce++;
        if (bounce < _settings.maxBounces)
            didHit = _scene->intersect(ray, data, info);
    }
}

Vec3f PhotonTracer::traceSample(Vec2u pixel, const KdTree<Photon> &surfaceTree,
        const KdTree<VolumePhoton> *mediumTree, SampleGenerator &sampler,
        UniformSampler &supplementalSampler, float gatherRadius)
{
    Ray ray;
    Vec3f throughput(1.0f);
    if (!_scene->cam().generateSample(pixel, sampler, throughput, ray))
        return Vec3f(0.0f);
    ray.setPrimaryRay(true);

    IntersectionTemporary data;
    IntersectionInfo info;
    const Medium *medium = _scene->cam().medium().get();

    Vec3f result(0.0f);
    int bounce = 0;
    bool didHit = _scene->intersect(ray, data, info);
    while ((medium || didHit) && bounce < _settings.maxBounces - 1) {
        ray.advanceFootprint();

        if (medium) {
            VolumeScatterEvent event(ray.pos(), ray.dir(), ray.farT());

            if (mediumTree) {
                Vec3f beamEstimate(0.0f);
                mediumTree->beamQuery(ray.pos(), ray.dir(), ray.farT(), [&](const VolumePhoton &p, float t, float distSq) {
                    event.t = t;
                    event.wo = -p.dir;
                    float kernel = (3.0f*INV_PI*sqr(1.0f - distSq/p.radiusSq))/p.radiusSq;
                    //float kernel = INV_PI/p.radiusSq;
                    beamEstimate += kernel*medium->phaseEval(event)*medium->transmittance(event)*p.power;
                });
                result += throughput*beamEstimate;
            }

            event.t = ray.farT();
            throughput *= medium->transmittance(event);
        }
        if (!didHit)
            break;

        const Bsdf &bsdf = *info.bsdf;

        SurfaceScatterEvent event = makeLocalScatterEvent(data, info, ray, &sampler, &supplementalSampler);

        Vec3f transparency = bsdf.eval(event.makeForwardEvent());
        float transparencyScalar = transparency.avg();

        Vec3f wo;
        float pdf;
        if (sampler.next1D() < transparencyScalar) {
            pdf = 0.0f;
            wo = ray.dir();
            throughput *= transparency/transparencyScalar;
        } else {
            event.requestedLobe = BsdfLobes::SpecularLobe;
            if (!bsdf.sample(event))
                break;

            pdf = event.pdf;
            wo = event.frame.toGlobal(event.wo);

            throughput *= event.throughput;
        }

        bool geometricBackside = (wo.dot(info.Ng) < 0.0f);
        const Medium *newMedium = medium;
        if (bsdf.overridesMedia()) {
            if (geometricBackside)
                newMedium = bsdf.intMedium().get();
            else
                newMedium = bsdf.extMedium().get();
        }
        medium = newMedium;

        ray = ray.scatter(ray.hitpoint(), wo, info.epsilon, pdf);

        if (std::isnan(ray.dir().sum() + ray.pos().sum()))
            break;
        if (std::isnan(throughput.sum()))
            break;

        bounce++;
        if (bounce < _settings.maxBounces)
            didHit = _scene->intersect(ray, data, info);
    }

    if (!didHit) {
        if (!medium && _scene->intersectInfinites(ray, data, info))
            result += throughput*info.primitive->emission(data, info);
        return result;
    }
    result += throughput*info.primitive->emission(data, info);

    int count = surfaceTree.nearestNeighbours(ray.hitpoint(), _photonQuery.get(), _distanceQuery.get(),
            _settings.gatherCount, gatherRadius);
    if (count == 0)
        return result;

    const Bsdf &bsdf = *info.bsdf;
    SurfaceScatterEvent event = makeLocalScatterEvent(data, info, ray, &sampler, &supplementalSampler);

    Vec3f surfaceEstimate(0.0f);
    for (int i = 0; i < count; ++i) {
        event.wo = event.frame.toLocal(-_photonQuery[i]->dir);
        surfaceEstimate += _photonQuery[i]->power*bsdf.eval(event)/std::abs(event.wo.z());
    }
    float radiusSq = count == int(_settings.gatherCount) ? _distanceQuery[0] : gatherRadius*gatherRadius;
    result += throughput*surfaceEstimate*(INV_PI/radiusSq);

    return result;
}

}
