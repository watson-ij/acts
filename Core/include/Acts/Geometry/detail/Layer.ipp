// This file is part of the Acts project.
//
// Copyright (C) 2016-2018 CERN for the benefit of the Acts project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <limits>

namespace Acts {

inline const SurfaceArray* Layer::surfaceArray() const {
  return m_surfaceArray.get();
}

inline SurfaceArray* Layer::surfaceArray() {
  return const_cast<SurfaceArray*>(m_surfaceArray.get());
}

inline double Layer::thickness() const {
  return m_layerThickness;
}

inline LayerType Layer::layerType() const {
  return m_layerType;
}

inline const TrackingVolume* Layer::trackingVolume() const {
  return m_trackingVolume;
}

inline void Layer::encloseTrackingVolume(const TrackingVolume& tvol) {
  m_trackingVolume = &(tvol);
}

inline const AbstractVolume* Layer::representingVolume() const {
  return m_representingVolume.get();
}

inline const Layer* Layer::nextLayer(const GeometryContext& /*gctx*/,
                                     const Vector3D& position,
                                     const Vector3D& direction) const {
  // no binutility -> no chance to find out the direction
  if (m_nextLayerUtility == nullptr) {
    return nullptr;
  }
  return (m_nextLayerUtility->nextDirection(position, direction) < 0)
             ? m_nextLayers.first
             : m_nextLayers.second;
}

inline bool Layer::resolve(bool resolveSensitive, bool resolveMaterial,
                           bool resolvePassive) const {
  if (resolvePassive) {
    return true;
  }
  if (resolveSensitive && m_surfaceArray) {
    return true;
  }
  if (resolveMaterial &&
      (m_ssSensitiveSurfaces > 1 || m_ssApproachSurfaces > 1 ||
       (surfaceRepresentation().surfaceMaterial() != nullptr))) {
    return true;
  }
  return false;
}

template <typename options_t>
std::vector<SurfaceIntersection> Layer::compatibleSurfaces(
    const GeometryContext& gctx, const Vector3D& position,
    const Vector3D& direction, const options_t& options) const {
  // the list of valid intersection
  std::vector<SurfaceIntersection> sIntersections;
  // remember the surfaces for duplicate removal
  std::map<const Surface*, bool> accepted;

  // fast exit - there is nothing to
  if (!m_surfaceArray || !m_approachDescriptor || !options.navDir) {
    return sIntersections;
  }

  // reserve a few bins
  sIntersections.reserve(20);

  // (0) End surface check
  // @todo: - we might be able to skip this by use of options.pathLimit
  // check if you have to stop at the endSurface
  double maxPath = options.pathLimit;
  if (options.endObject) {
    // intersect the end surface
    // - it is the final one don't use the bounday check at all
    SurfaceIntersection endInter = options.endObject->intersect(
        gctx, position, options.navDir * direction, BoundaryCheck(true));
    // non-valid intersection with the end surface provided at this layer
    // indicates wrong direction or faulty setup
    // -> do not return compatible surfaces since they may lead you on a wrong
    // navigation path
    if (endInter) {
      maxPath = endInter.intersection.pathLength;
    } else {
      return sIntersections;
    }

  } else {
    // compatibleSurfaces() should only be called when on the layer,
    // i.e. the maximum path limit is given by the layer thickness times
    // path correction, we take a safety factor of 1.5
    // -> this avoids punch through for cylinders
    double pCorrection =
        surfaceRepresentation().pathCorrection(gctx, position, direction);
    maxPath = 1.5 * thickness() * pCorrection * options.navDir;
  }

  // lemma 0 : accept the surface
  auto acceptSurface = [&options, &accepted](const Surface& sf,
                                             bool sensitive = false) -> bool {
    // check for duplicates
    if (accepted.find(&sf) != accepted.end()) {
      return false;
    }
    // surface is sensitive and you're asked to resolve
    if (sensitive && options.resolveSensitive) {
      return true;
    }
    // next option: it's a material surface and you want to have it
    if (options.resolveMaterial && sf.surfaceMaterial()) {
      return true;
    }
    // last option: resovle all
    return options.resolvePassive;
  };

  // lemma 1 : check and fill the surface
  // [&sIntersections, &options, &parameters
  auto processSurface = [&](const Surface& sf, bool sensitive = false) {
    // veto if it's start or end surface
    if (options.startObject == &sf || options.endObject == &sf) {
      return;
    }
    // veto if it doesn't fit the prescription
    if (!acceptSurface(sf, sensitive)) {
      return;
    }
    // the surface intersection
    SurfaceIntersection sfi = sf.intersect(
        gctx, position, options.navDir * direction, options.boundaryCheck);
    // check if intersection is valid and pathLimit has not been exceeded
    double sifPath = sfi.intersection.pathLength;
    // check the maximum path length
    if (sfi && sifPath * sifPath <= maxPath * maxPath) {
      sIntersections.push_back(sfi);
      accepted[&sf] = true;
    }
    return;
  };

  // (A) approach descriptor section
  //
  // the approach surfaces are in principle always testSurfaces
  // - the surface on approach is excluded via the veto
  // - the surfaces are only collected if needed
  if (m_approachDescriptor &&
      (options.resolveMaterial || options.resolvePassive)) {
    // the approach surfaces
    const std::vector<const Surface*>& approachSurfaces =
        m_approachDescriptor->containedSurfaces();
    // we loop through and veto
    // - if the approach surface is the parameter surface
    // - if the surface is not compatible with the collect
    for (auto& aSurface : approachSurfaces) {
      processSurface(*aSurface);
    }
  }

  // (B) sensitive surface section
  //
  // check the sensitive surfaces if you have some
  if (m_surfaceArray && (options.resolveMaterial || options.resolvePassive ||
                         options.resolveSensitive)) {
    // get the canditates
    const std::vector<const Surface*>& sensitiveSurfaces =
        m_surfaceArray->neighbors(position);
    // loop through and veto
    // - if the approach surface is the parameter surface
    // - if the surface is not compatible with the type(s) that are collected
    for (auto& sSurface : sensitiveSurfaces) {
      processSurface(*sSurface, true);
    }
  }

  // (C) representing surface section
  //
  // the layer surface itself is a testSurface
  const Surface* layerSurface = &surfaceRepresentation();
  processSurface(*layerSurface);

  // sort according to the path length
  if (options.navDir == forward) {
    std::sort(sIntersections.begin(), sIntersections.end());
  } else {
    std::sort(sIntersections.begin(), sIntersections.end(), std::greater<>());
  }

  return sIntersections;
}

template <typename options_t>
const SurfaceIntersection Layer::surfaceOnApproach(
    const GeometryContext& gctx, const Vector3D& position,
    const Vector3D& direction, const options_t& options) const {
  // resolve directive based by options
  // - options.resolvePassive is on -> always
  // - options.resolveSensitive is on -> always
  // - options.resolveMaterial is on
  //   && either sensitive or approach surfaces have material
  bool resolvePS = options.resolveSensitive || options.resolvePassive;
  bool resolveMS = options.resolveMaterial &&
                   (m_ssSensitiveSurfaces > 1 || m_ssApproachSurfaces > 1 ||
                    surfaceRepresentation().surfaceMaterial());

  // now of course this only counts when you have an approach descriptor
  if (m_approachDescriptor && (resolvePS || resolveMS)) {
    // Test if you are on an approach surface already, if so - provide it
    for (auto& asf : m_approachDescriptor->containedSurfaces()) {
      // In a connected geometry this is only a pointer comparison
      if (options.startObject &&
          asf == &(options.startObject->surfaceRepresentation())) {
        Intersection nIntersection(position, 0.,
                                   Intersection::Status::onSurface);
        return SurfaceIntersection(nIntersection, asf);
      }
    }
    // that's the collect trigger for always collecting
    // let's find the most suitable approach surface
    SurfaceIntersection aSurface = m_approachDescriptor->approachSurface(
        gctx, position, options.navDir * direction, options.boundaryCheck);
    if (bool(aSurface)) {
      return (aSurface);
    }
  }

  const Surface& rSurface = surfaceRepresentation();

  // if we have no approach descriptor - we have no sensitive surfaces
  if (rSurface.isOnSurface(gctx, position, direction, options.boundaryCheck)) {
    Intersection nIntersection(position, 0., Intersection::Status::onSurface);
    return SurfaceIntersection(nIntersection, &rSurface);
  }

  // create the intersection with the surface representation
  return rSurface.intersect(gctx, position, options.navDir * direction,
                            options.boundaryCheck);
}

inline bool Layer::isOnLayer(const GeometryContext& gctx,
                             const Vector3D& position,
                             const BoundaryCheck& bcheck) const {
  if (m_representingVolume != nullptr) {
    return m_representingVolume->inside(position);
  }
  return (surfaceRepresentation())
      .isOnSurface(gctx, position, s_origin, bcheck);
}

}  // namespace Acts
