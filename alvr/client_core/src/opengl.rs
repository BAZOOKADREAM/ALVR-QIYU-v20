// OpenGL rendering backend for the client_core C API.
//
// ALVR v20 replaced the old C++ OpenGL renderer with a wgpu-based renderer. That renderer creates
// its own EGL context internally, which is incompatible with the standalone C++ clients that use
// this C API (the app-provided swapchain textures live in a different EGL context). This module
// restores the proven v19 C++ renderer (gl_render_utils + tinygltf), which renders directly into
// the caller's current EGL context. The same approach is used by the PhoneVR v20 fork.

#![allow(unused_variables)]

use alvr_common::{
    glam::UVec2, once_cell::sync::Lazy, parking_lot::Mutex, Fov, Pose,
};
use alvr_session::FoveatedEncodingConfig;
use glyph_brush_layout::{
    ab_glyph::{Font, FontRef, ScaleFont},
    FontId, GlyphPositioner, HorizontalAlign, Layout, SectionGeometry, SectionText, VerticalAlign,
};
use std::ffi::c_void;

const HUD_TEXTURE_WIDTH: usize = 1280;
const HUD_TEXTURE_HEIGHT: usize = 720;
const FONT_SIZE: f32 = 50_f32;

static HUD_MESSAGE: Lazy<Mutex<String>> = Lazy::new(|| Mutex::new(String::new()));
static HUD_RENDERED: Lazy<Mutex<String>> = Lazy::new(|| Mutex::new(String::new()));

#[repr(C)]
#[derive(Clone, Copy)]
struct FfiViewInput {
    orientation: [f32; 4], // x, y, z, w
    position: [f32; 3],
    fov_left: f32,
    fov_right: f32,
    fov_up: f32,
    fov_down: f32,
    swapchain_index: u32,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct FfiStreamConfig {
    view_width: u32,
    view_height: u32,
    swapchain_textures: [*const u32; 2],
    swapchain_length: u32,
    enable_foveation: u32,
    foveation_center_size_x: f32,
    foveation_center_size_y: f32,
    foveation_center_shift_x: f32,
    foveation_center_shift_y: f32,
    foveation_edge_ratio_x: f32,
    foveation_edge_ratio_y: f32,
    enable_srgb_correction: u32,
}

#[cfg(target_os = "android")]
unsafe extern "C" {
    static mut LOBBY_ROOM_GLTF_PTR: *const u8;
    static mut LOBBY_ROOM_GLTF_LEN: u32;
    static mut LOBBY_ROOM_BIN_PTR: *const u8;
    static mut LOBBY_ROOM_BIN_LEN: u32;

    fn initGraphicsNative();
    fn destroyGraphicsNative();
    fn prepareLobbyRoom(
        view_width: i32,
        view_height: i32,
        swapchain_textures: *mut *const u32,
        swapchain_length: i32,
    );
    fn destroyRenderers();
    fn streamStartNative(config: FfiStreamConfig);
    fn updateLobbyHudTexture(data: *const u8);
    fn renderLobbyNative(eye_inputs: *const FfiViewInput);
    fn renderStreamNative(stream_hardware_buffer: *mut c_void, swapchain_indices: *const u32);
}

pub struct RenderViewInput {
    pub pose: Pose,
    pub fov: Fov,
    pub swapchain_index: u32,
}

pub fn initialize() {
    #[cfg(target_os = "android")]
    unsafe {
        static LOBBY_ROOM_GLTF: &[u8] = include_bytes!("../resources/loading.gltf");
        static LOBBY_ROOM_BIN: &[u8] = include_bytes!("../resources/buffer.bin");

        LOBBY_ROOM_GLTF_PTR = LOBBY_ROOM_GLTF.as_ptr();
        LOBBY_ROOM_GLTF_LEN = LOBBY_ROOM_GLTF.len() as _;
        LOBBY_ROOM_BIN_PTR = LOBBY_ROOM_BIN.as_ptr();
        LOBBY_ROOM_BIN_LEN = LOBBY_ROOM_BIN.len() as _;

        initGraphicsNative();
    }
}

pub fn destroy() {
    #[cfg(target_os = "android")]
    unsafe {
        destroyGraphicsNative();
    }
}

pub fn resume(preferred_view_resolution: UVec2, swapchain_textures: [Vec<u32>; 2]) {
    #[cfg(target_os = "android")]
    unsafe {
        let swapchain_length = swapchain_textures[0].len();
        let mut swapchain_textures = [
            swapchain_textures[0].as_ptr(),
            swapchain_textures[1].as_ptr(),
        ];

        prepareLobbyRoom(
            preferred_view_resolution.x as _,
            preferred_view_resolution.y as _,
            swapchain_textures.as_mut_ptr(),
            swapchain_length as _,
        );
    }
}

pub fn pause() {
    #[cfg(target_os = "android")]
    unsafe {
        destroyRenderers();
    }
}

pub fn start_stream(
    view_resolution: UVec2,
    swapchain_textures: [Vec<u32>; 2],
    foveated_encoding: Option<FoveatedEncodingConfig>,
    enable_srgb_correction: bool,
) {
    #[cfg(target_os = "android")]
    unsafe {
        let config = FfiStreamConfig {
            view_width: view_resolution.x,
            view_height: view_resolution.y,
            swapchain_textures: [
                swapchain_textures[0].as_ptr(),
                swapchain_textures[1].as_ptr(),
            ],
            swapchain_length: swapchain_textures[0].len() as _,
            enable_foveation: foveated_encoding.is_some().into(),
            foveation_center_size_x: foveated_encoding
                .as_ref()
                .map(|f| f.center_size_x)
                .unwrap_or_default(),
            foveation_center_size_y: foveated_encoding
                .as_ref()
                .map(|f| f.center_size_y)
                .unwrap_or_default(),
            foveation_center_shift_x: foveated_encoding
                .as_ref()
                .map(|f| f.center_shift_x)
                .unwrap_or_default(),
            foveation_center_shift_y: foveated_encoding
                .as_ref()
                .map(|f| f.center_shift_y)
                .unwrap_or_default(),
            foveation_edge_ratio_x: foveated_encoding
                .as_ref()
                .map(|f| f.edge_ratio_x)
                .unwrap_or_default(),
            foveation_edge_ratio_y: foveated_encoding
                .as_ref()
                .map(|f| f.edge_ratio_y)
                .unwrap_or_default(),
            enable_srgb_correction: enable_srgb_correction as u32,
        };

        streamStartNative(config);
    }
}

pub fn update_hud_message(message: &str) {
    *HUD_MESSAGE.lock() = message.to_owned();
}

// The C++ renderer issues GL calls, so the HUD texture upload must happen on the rendering thread
// (the only thread with a current EGL context). `update_hud_message` may be called from any
// thread; it just stores the text and the next `render_lobby` call uploads it lazily.
fn upload_hud_if_needed() {
    let message = HUD_MESSAGE.lock().clone();
    if *HUD_RENDERED.lock() == message {
        return;
    }

    let ubuntu_font =
        FontRef::try_from_slice(include_bytes!("../resources/Ubuntu-Medium.ttf")).unwrap();

    let section_glyphs = Layout::default()
        .h_align(HorizontalAlign::Center)
        .v_align(VerticalAlign::Center)
        .calculate_glyphs(
            &[&ubuntu_font],
            &SectionGeometry {
                screen_position: (
                    HUD_TEXTURE_WIDTH as f32 / 2_f32,
                    HUD_TEXTURE_HEIGHT as f32 / 2_f32,
                ),
                ..Default::default()
            },
            &[SectionText {
                text: &message,
                scale: FONT_SIZE.into(),
                font_id: FontId(0),
            }],
        );

    let scaled_font = ubuntu_font.as_scaled(FONT_SIZE);

    let mut buffer = vec![0_u8; HUD_TEXTURE_WIDTH * HUD_TEXTURE_HEIGHT * 4];

    for section_glyph in section_glyphs {
        if let Some(outlined) = scaled_font.outline_glyph(section_glyph.glyph) {
            let bounds = outlined.px_bounds();
            outlined.draw(|x, y, alpha| {
                let x = x as usize + bounds.min.x as usize;
                let y = y as usize + bounds.min.y as usize;
                buffer[(y * HUD_TEXTURE_WIDTH + x) * 4 + 3] = (alpha * 255.0) as u8;
            });
        }
    }

    #[cfg(target_os = "android")]
    unsafe {
        updateLobbyHudTexture(buffer.as_ptr());
    }

    *HUD_RENDERED.lock() = message;
}

pub fn render_lobby(view_inputs: [RenderViewInput; 2]) {
    #[cfg(target_os = "android")]
    {
        upload_hud_if_needed();

        unsafe {
            let eye_inputs = [
                FfiViewInput {
                    position: view_inputs[0].pose.position.to_array(),
                    orientation: view_inputs[0].pose.orientation.to_array(),
                    fov_left: view_inputs[0].fov.left,
                    fov_right: view_inputs[0].fov.right,
                    fov_up: view_inputs[0].fov.up,
                    fov_down: view_inputs[0].fov.down,
                    swapchain_index: view_inputs[0].swapchain_index,
                },
                FfiViewInput {
                    position: view_inputs[1].pose.position.to_array(),
                    orientation: view_inputs[1].pose.orientation.to_array(),
                    fov_left: view_inputs[1].fov.left,
                    fov_right: view_inputs[1].fov.right,
                    fov_up: view_inputs[1].fov.up,
                    fov_down: view_inputs[1].fov.down,
                    swapchain_index: view_inputs[1].swapchain_index,
                },
            ];

            renderLobbyNative(eye_inputs.as_ptr());
        }
    }
}

pub fn render_stream(hardware_buffer: *mut c_void, swapchain_indices: [u32; 2]) {
    #[cfg(target_os = "android")]
    unsafe {
        renderStreamNative(hardware_buffer, swapchain_indices.as_ptr());
    }
}
