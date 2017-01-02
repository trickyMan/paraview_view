StreamLinesRepresentation plugin for ParaView

by B.Jacquet & J.Pouderoux, Kitware SAS 2017

TODO and bugs:
- Support Structured grids
- Support Unstructured grids
- Auto re-render without needing loop
- Fix: Buffer get garbage when view is resized (on some machines only)

Extended features:
- For non-AABB (based on vol(domain)/vol(AABB)< 0.2): better point sampling via cell-sampling using cumsum of cell-volume
- Nice volume rendering of mag(speed data) (with correct depth/alpha)
- Correct VolumeMapper to show mag if vector data
