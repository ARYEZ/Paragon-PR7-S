# Paragon PR7-S's patch to JUCE (0.76.1), run in JUCE's source folder after it's fetched.
#
# DRAW WITHOUT THE GRAPHICS CARD (SETTINGS) must keep the graphics card's driver off the screen's thread
# altogether: the owner's freezes were inside the NVIDIA driver (d2d1 / d3d11 / nvwgf2umx). JUCE on Windows
# gives every new window a Direct2D renderer and makes every ordinary image a Direct2D image, whatever the
# window draws with, so switching windows afterwards (0.60.0, 0.73.4) still left Direct2D work behind: each
# new window (a menu, the render's progress box) made and tore down a Direct2D context, and images (shadows,
# the studio's own) lived on the card. With `paragonPreferSoftwareRenderer` set, new windows are made with
# the software renderer and new images in memory, as JUCE already does when running under Wine.
#
# Each replacement must find its text exactly once, else the build stops: a newer JUCE needs this looked at.

function (paragon_patch file before after marker)
    file (READ "${file}" text)
    string (FIND "${text}" "${marker}" already)
    if (NOT already EQUAL -1)
        return()   # (patched already)
    endif()
    string (FIND "${text}" "${before}" at)
    if (at EQUAL -1)
        message (FATAL_ERROR "PatchJuce: the text to patch wasn't found in ${file}")
    endif()
    string (REPLACE "${before}" "${after}" text "${text}")
    file (WRITE "${file}" "${text}")
endfunction()

paragon_patch ("modules/juce_graphics/native/juce_Direct2DImage_windows.cpp"
"ImagePixelData::Ptr NativeImageType::create (Image::PixelFormat format, int width, int height, bool clearImage) const
{
    if (! juce_isRunningInWine())"
"std::atomic<bool> paragonPreferSoftwareRenderer { false };   // (Paragon PR7-S: DRAW WITHOUT THE GRAPHICS CARD)

ImagePixelData::Ptr NativeImageType::create (Image::PixelFormat format, int width, int height, bool clearImage) const
{
    if (! juce_isRunningInWine() && ! paragonPreferSoftwareRenderer.load())"
"paragonPreferSoftwareRenderer { false }")

paragon_patch ("modules/juce_gui_basics/native/juce_Windowing_windows.cpp"
"ComponentPeer* Component::createNewPeer (int styleFlags, void* parentHWND)
{
    const auto renderer = juce_isRunningInWine() ? 0 : 1;"
"// (Paragon PR7-S: defined in juce_Direct2DImage_windows.cpp; declared at namespace scope, not inside the
// function: MSVC put a block-scope extern in the global namespace and the link failed.)
extern std::atomic<bool> paragonPreferSoftwareRenderer;

ComponentPeer* Component::createNewPeer (int styleFlags, void* parentHWND)
{
    const auto renderer = juce_isRunningInWine() || paragonPreferSoftwareRenderer.load() ? 0 : 1;"
"extern std::atomic<bool> paragonPreferSoftwareRenderer;

ComponentPeer")

# (0.78.1) The owner's 0.78.0 log: frozen again after Render in Place's disable, the screen's thread inside
# d2d1 / d3d11 / nvwgf2umx with software drawing on. Every window (software ones too) joins JUCE's display-sync
# (VBlankDispatcher), which owns the DirectX singleton; that made a Direct3D 11 device and a Direct2D device on
# every graphics adapter, and released and remade them on the screen's thread whenever Windows reported a
# change (WM_SETTINGCHANGE / WM_DISPLAYCHANGE, or the DXGI factory no longer current). The display sync needs
# only the adapters' outputs (WaitForVBlank), not a device, so with `paragonPreferSoftwareRenderer` the
# adapters are listed without devices: nothing on the graphics card's driver at all. Switching back to the
# graphics card makes the devices (`paragonRefreshDirectX`, called by Windows::applyDrawing).
paragon_patch ("modules/juce_graphics/native/juce_DirectX_windows.cpp"
"auto DxgiAdapter::create (ComSmartPtr<ID2D1Factory2> d2dFactory,"
"extern std::atomic<bool> paragonPreferSoftwareRenderer;   // (Paragon PR7-S: juce_Direct2DImage_windows.cpp)

auto DxgiAdapter::create (ComSmartPtr<ID2D1Factory2> d2dFactory,"
"extern std::atomic<bool> paragonPreferSoftwareRenderer;   // (Paragon PR7-S: juce_Direct2DImage_windows.cpp)")

paragon_patch ("modules/juce_graphics/native/juce_DirectX_windows.cpp"
"    // This flag adds support for surfaces with a different color channel ordering"
"    if (paragonPreferSoftwareRenderer.load())   // (Paragon PR7-S: the outputs only, no device on the card)
        return result;

    // This flag adds support for surfaces with a different color channel ordering"
"(Paragon PR7-S: the outputs only, no device on the card)")

paragon_patch ("modules/juce_graphics/native/juce_DirectX_windows.cpp"
"    if (factory != nullptr && factory->IsCurrent() && ! adapterArray.isEmpty())
        return;"
"    if (factory != nullptr && factory->IsCurrent() && ! adapterArray.isEmpty()
        && (paragonPreferSoftwareRenderer.load() || adapterArray.getFirst()->direct3DDevice != nullptr))   // (Paragon PR7-S)
        return;"
"adapterArray.getFirst()->direct3DDevice != nullptr))   // (Paragon PR7-S)")

paragon_patch ("modules/juce_graphics/native/juce_DirectX_windows.cpp"
"DxgiAdapters::DxgiAdapters (ComSmartPtr<ID2D1Factory2> d2dFactoryIn)"
"// (Paragon PR7-S) Back to the graphics card: the adapters made again with their devices.
void paragonRefreshDirectX()
{
    SharedResourcePointer<DirectX> directX;
    directX->adapters.updateAdapters();
}

DxgiAdapters::DxgiAdapters (ComSmartPtr<ID2D1Factory2> d2dFactoryIn)"
"void paragonRefreshDirectX()")
