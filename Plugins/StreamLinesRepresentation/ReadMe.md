

TODO and bugs:
- structured grid
- unstructured grid
- buffer get garbage when PV window is resize
- auto re-render without needing loop

Extended features:
- For non-AABB (based on vol(domain)/vol(AABB)< 0.2): better point sampling via cell-sampling using cumsum of cell-volume
- Nice volume rendering of mag(speed data) ()with correct depth/alpha)
- correct VolumeMapper to show mag if vector data
