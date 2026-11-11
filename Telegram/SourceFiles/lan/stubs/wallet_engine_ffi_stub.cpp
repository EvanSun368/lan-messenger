/* Auto-generated uniffi FFI stubs so TON wallet-engine Rust is not required.
 * Checksums and contract version match the bundled wallet_engine.cpp bindings.
 * Non-checksum FFI returns errors or empty data. LAN text chat does not use this.
 */
#include "wallet_engine_scaffolding.hpp"
#include <cstdlib>
#include <cstring>

extern "C" {

uint64_t uniffi_wallet_engine_fn_clone_walletclient(uint64_t handle, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return 0;
}

void uniffi_wallet_engine_fn_free_walletclient(uint64_t handle, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
}

uint64_t uniffi_wallet_engine_fn_constructor_walletclient_new(RustBuffer config, uint64_t http_host, uint64_t platform_host, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return 0;
}

uint64_t uniffi_wallet_engine_fn_constructor_walletclient_new_statusless(RustBuffer config, uint64_t statusless_host, uint64_t platform_host, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletclient_cancel_load_more_activity(uint64_t ptr) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletclient_load_more_activity(uint64_t ptr) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletclient_shutdown(uint64_t ptr) {
    return 0;
}

RustBuffer uniffi_wallet_engine_fn_method_walletclient_snapshot(uint64_t ptr, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

uint64_t uniffi_wallet_engine_fn_method_walletclient_wait_for_change(uint64_t ptr, uint64_t after_revision) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletclient_resolve_dns(uint64_t ptr, RustBuffer name) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletclient_create_encrypted_comment(uint64_t ptr, RustBuffer request) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletclient_decrypt_comment(uint64_t ptr, RustBuffer request) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletclient_resolve_encrypted_comment_recipient(uint64_t ptr, RustBuffer request) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletclient_prepare_key_rotation(uint64_t ptr, RustBuffer request) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletclient_cancel_load_more_nfts(uint64_t ptr) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletclient_cancel_refresh_nfts(uint64_t ptr) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletclient_load_more_nfts(uint64_t ptr) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletclient_refresh_nfts(uint64_t ptr) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletclient_preview_nft_transfer(uint64_t ptr, RustBuffer request) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletclient_send_nft_transfer(uint64_t ptr, RustBuffer request) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletclient_prepare_transfer(uint64_t ptr, RustBuffer request) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletclient_cancel_send_preview(uint64_t ptr) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletclient_preview_send(uint64_t ptr, RustBuffer request) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletclient_preview_send_boc(uint64_t ptr, RustBuffer request) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletclient_preview_sign_message(uint64_t ptr, RustBuffer request) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletclient_preview_ton_connect(uint64_t ptr, RustBuffer request) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletclient_cancel_refresh(uint64_t ptr) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletclient_refresh(uint64_t ptr) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletclient_resolve_pending(uint64_t ptr) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletclient_cancel_send(uint64_t ptr) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletclient_send(uint64_t ptr, RustBuffer request) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletclient_send_boc(uint64_t ptr, RustBuffer request) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletclient_sign_message(uint64_t ptr, RustBuffer request) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_clone_walletplatformhost(uint64_t handle, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return 0;
}

void uniffi_wallet_engine_fn_free_walletplatformhost(uint64_t handle, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
}

void uniffi_wallet_engine_fn_init_callback_vtable_walletplatformhost(const UniffiVTableCallbackInterfaceWalletPlatformHost & vtable) {
    (void)0;
}

uint64_t uniffi_wallet_engine_fn_method_walletplatformhost_read_protected_secret(uint64_t ptr, RustBuffer request) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletplatformhost_store_protected_secret(uint64_t ptr, RustBuffer request) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletplatformhost_delete_protected_secret(uint64_t ptr, RustBuffer secret_ref) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletplatformhost_load_journal(uint64_t ptr, RustBuffer key) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletplatformhost_compare_exchange_journal(uint64_t ptr, RustBuffer mutation) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_clone_tonconnectsession(uint64_t handle, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return 0;
}

void uniffi_wallet_engine_fn_free_tonconnectsession(uint64_t handle, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
}

RustBuffer uniffi_wallet_engine_fn_method_tonconnectsession_approve_connect(uint64_t ptr, RustBuffer account, RustBuffer proof, RustBuffer device, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_tonconnectsession_begin_events_subscription(uint64_t ptr, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

void uniffi_wallet_engine_fn_method_tonconnectsession_complete_pending_post(uint64_t ptr, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
}

RustBuffer uniffi_wallet_engine_fn_method_tonconnectsession_connect_prompt(uint64_t ptr, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_tonconnectsession_disconnect(uint64_t ptr, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_tonconnectsession_ingest_sse_chunk(uint64_t ptr, RustBuffer chunk, uint64_t now, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_tonconnectsession_pending_post(uint64_t ptr, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_tonconnectsession_pending_requests(uint64_t ptr, uint64_t now, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_tonconnectsession_persisted(uint64_t ptr, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_tonconnectsession_phase(uint64_t ptr, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_tonconnectsession_prepare_disconnect_success(uint64_t ptr, RustBuffer request_id, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_tonconnectsession_prepare_error(uint64_t ptr, RustBuffer request_id, RustBuffer code, RustBuffer message, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_tonconnectsession_prepare_send_success(uint64_t ptr, RustBuffer request_id, RustBuffer signed_boc, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_tonconnectsession_prepare_sign_message_success(uint64_t ptr, RustBuffer request_id, RustBuffer internal_boc, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_tonconnectsession_reject_connect(uint64_t ptr, RustBuffer message, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

uint64_t uniffi_wallet_engine_fn_clone_tonconnectderivedsession(uint64_t handle, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return 0;
}

void uniffi_wallet_engine_fn_free_tonconnectderivedsession(uint64_t handle, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
}

RustBuffer uniffi_wallet_engine_fn_method_tonconnectderivedsession_decode_sign_data_cell(uint64_t ptr, RustBuffer schema, RustBuffer cell, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_tonconnectderivedsession_decrypt_request(uint64_t ptr, RustBuffer body, uint64_t now, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_tonconnectderivedsession_encrypt_connect_error(uint64_t ptr, uint64_t event_id, RustBuffer code, RustBuffer message, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_tonconnectderivedsession_encrypt_connect_event(uint64_t ptr, uint64_t event_id, RustBuffer account, RustBuffer proof, RustBuffer device, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_tonconnectderivedsession_encrypt_disconnect_event(uint64_t ptr, uint64_t event_id, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_tonconnectderivedsession_encrypt_disconnect_success(uint64_t ptr, RustBuffer request_id, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_tonconnectderivedsession_encrypt_error(uint64_t ptr, RustBuffer request_id, RustBuffer code, RustBuffer message, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_tonconnectderivedsession_encrypt_send_success(uint64_t ptr, RustBuffer request_id, RustBuffer signed_boc, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_tonconnectderivedsession_encrypt_sign_data_success(uint64_t ptr, RustBuffer request_id, RustBuffer signed_data, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_tonconnectderivedsession_open_challenge(uint64_t ptr, RustBuffer challenge, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_tonconnectderivedsession_public_key_hex(uint64_t ptr, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_tonconnectderivedsession_signing_public_key(uint64_t ptr, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

uint64_t uniffi_wallet_engine_fn_clone_wallethttphost(uint64_t handle, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return 0;
}

void uniffi_wallet_engine_fn_free_wallethttphost(uint64_t handle, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
}

void uniffi_wallet_engine_fn_init_callback_vtable_wallethttphost(const UniffiVTableCallbackInterfaceWalletHttpHost & vtable) {
    (void)0;
}

uint64_t uniffi_wallet_engine_fn_method_wallethttphost_execute_http(uint64_t ptr, RustBuffer request) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_wallethttphost_cancel_http(uint64_t ptr, RustBuffer request_id) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_clone_walletstatuslesshost(uint64_t handle, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return 0;
}

void uniffi_wallet_engine_fn_free_walletstatuslesshost(uint64_t handle, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
}

void uniffi_wallet_engine_fn_init_callback_vtable_walletstatuslesshost(const UniffiVTableCallbackInterfaceWalletStatuslessHost & vtable) {
    (void)0;
}

uint64_t uniffi_wallet_engine_fn_method_walletstatuslesshost_execute_statusless(uint64_t ptr, RustBuffer request) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletstatuslesshost_cancel_statusless(uint64_t ptr, RustBuffer request_id) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_clone_walletlifecycle(uint64_t handle, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return 0;
}

void uniffi_wallet_engine_fn_free_walletlifecycle(uint64_t handle, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
}

uint64_t uniffi_wallet_engine_fn_constructor_walletlifecycle_new(uint64_t platform_host, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletlifecycle_create_wallet(uint64_t ptr, RustBuffer request) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletlifecycle_delete_wallet(uint64_t ptr, RustBuffer descriptor) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletlifecycle_derive_ton_connect_session(uint64_t ptr, RustBuffer request) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletlifecycle_import_wallet(uint64_t ptr, RustBuffer request) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletlifecycle_reveal_recovery_phrase(uint64_t ptr, RustBuffer descriptor) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletlifecycle_sign_ton_connect_data(uint64_t ptr, RustBuffer request) {
    return 0;
}

uint64_t uniffi_wallet_engine_fn_method_walletlifecycle_sign_ton_connect_proof(uint64_t ptr, RustBuffer request) {
    return 0;
}

RustBuffer uniffi_wallet_engine_fn_method_walletlifecycle_ton_connect_account(uint64_t ptr, RustBuffer descriptor, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_walletclienterror_uniffi_trait_display(RustBuffer ptr, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_journalhosterror_uniffi_trait_display(RustBuffer ptr, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_protectedsecrethosterror_uniffi_trait_display(RustBuffer ptr, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_tonconnectsessionerror_uniffi_trait_display(RustBuffer ptr, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_httphosterror_uniffi_trait_display(RustBuffer ptr, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_statuslesshosterror_uniffi_trait_display(RustBuffer ptr, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_tonaddresserror_uniffi_trait_display(RustBuffer ptr, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_method_walletlifecycleerror_uniffi_trait_display(RustBuffer ptr, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_func_parse_ton_transfer_link(RustBuffer value, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_func_parse_ton_connect_manifest(RustBuffer json, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

uint64_t uniffi_wallet_engine_fn_func_ton_connect_session_from_link(RustBuffer link, RustBuffer config, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return 0;
}

uint64_t uniffi_wallet_engine_fn_func_ton_connect_session_restore(RustBuffer persisted, RustBuffer config, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return 0;
}

RustBuffer uniffi_wallet_engine_fn_func_convert_ton_address(RustBuffer value, RustBuffer format, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

int8_t uniffi_wallet_engine_fn_func_is_valid_ton_address(RustBuffer value, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return 0;
}

RustBuffer uniffi_wallet_engine_fn_func_parse_ton_address(RustBuffer value, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_func_mnemonic_wordlist(RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_func_detect_mnemonic_schemes(RustBuffer words, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer uniffi_wallet_engine_fn_func_rotation_mnemonic_public_key(RustBuffer phrase, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

RustBuffer ffi_wallet_engine_rustbuffer_alloc(uint64_t size, RustCallStatus *out_status) {
    RustBuffer buf{};
    if (size == 0) {
        return buf;
    }
    buf.data = static_cast<uint8_t*>(std::malloc(size));
    buf.capacity = size;
    buf.len = size;
    if (out_status) {
        out_status->code = buf.data ? 0 : 1;
        out_status->error_buf = RustBuffer{};
    }
    return buf;
}

RustBuffer ffi_wallet_engine_rustbuffer_from_bytes(ForeignBytes bytes, RustCallStatus *out_status) {
    RustBuffer buf{};
    const auto n = bytes.len > 0 ? static_cast<uint64_t>(bytes.len) : 0;
    if (n && bytes.data) {
        buf.data = static_cast<uint8_t*>(std::malloc(n));
        if (buf.data) {
            std::memcpy(buf.data, bytes.data, static_cast<size_t>(n));
            buf.capacity = n;
            buf.len = n;
        }
    }
    if (out_status) {
        out_status->code = (n && !buf.data) ? 1 : 0;
        out_status->error_buf = RustBuffer{};
    }
    return buf;
}

void ffi_wallet_engine_rustbuffer_free(RustBuffer buf, RustCallStatus *out_status) {
    if (buf.data) {
        std::free(buf.data);
    }
    if (out_status) {
        out_status->code = 0;
        out_status->error_buf = RustBuffer{};
    }
}

RustBuffer ffi_wallet_engine_rustbuffer_reserve(RustBuffer buf, uint64_t additional, RustCallStatus *out_status) {
    if (additional > 0) {
        const auto new_cap = buf.capacity + additional;
        auto *p = static_cast<uint8_t*>(std::realloc(buf.data, static_cast<size_t>(new_cap)));
        if (p) {
            buf.data = p;
            buf.capacity = new_cap;
        } else if (out_status) {
            out_status->code = 1;
            out_status->error_buf = RustBuffer{};
            return buf;
        }
    }
    if (out_status) {
        out_status->code = 0;
        out_status->error_buf = RustBuffer{};
    }
    return buf;
}

void ffi_wallet_engine_rust_future_poll_u8(uint64_t handle, void * callback, uint64_t callback_data) {
    (void)0;
}

void ffi_wallet_engine_rust_future_cancel_u8(uint64_t handle) {
    (void)0;
}

void ffi_wallet_engine_rust_future_free_u8(uint64_t handle) {
    (void)0;
}

uint8_t ffi_wallet_engine_rust_future_complete_u8(uint64_t handle, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return 0;
}

void ffi_wallet_engine_rust_future_poll_i8(uint64_t handle, void * callback, uint64_t callback_data) {
    (void)0;
}

void ffi_wallet_engine_rust_future_cancel_i8(uint64_t handle) {
    (void)0;
}

void ffi_wallet_engine_rust_future_free_i8(uint64_t handle) {
    (void)0;
}

int8_t ffi_wallet_engine_rust_future_complete_i8(uint64_t handle, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return 0;
}

void ffi_wallet_engine_rust_future_poll_u16(uint64_t handle, void * callback, uint64_t callback_data) {
    (void)0;
}

void ffi_wallet_engine_rust_future_cancel_u16(uint64_t handle) {
    (void)0;
}

void ffi_wallet_engine_rust_future_free_u16(uint64_t handle) {
    (void)0;
}

uint16_t ffi_wallet_engine_rust_future_complete_u16(uint64_t handle, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return 0;
}

void ffi_wallet_engine_rust_future_poll_i16(uint64_t handle, void * callback, uint64_t callback_data) {
    (void)0;
}

void ffi_wallet_engine_rust_future_cancel_i16(uint64_t handle) {
    (void)0;
}

void ffi_wallet_engine_rust_future_free_i16(uint64_t handle) {
    (void)0;
}

int16_t ffi_wallet_engine_rust_future_complete_i16(uint64_t handle, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return 0;
}

void ffi_wallet_engine_rust_future_poll_u32(uint64_t handle, void * callback, uint64_t callback_data) {
    (void)0;
}

void ffi_wallet_engine_rust_future_cancel_u32(uint64_t handle) {
    (void)0;
}

void ffi_wallet_engine_rust_future_free_u32(uint64_t handle) {
    (void)0;
}

uint32_t ffi_wallet_engine_rust_future_complete_u32(uint64_t handle, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return 0;
}

void ffi_wallet_engine_rust_future_poll_i32(uint64_t handle, void * callback, uint64_t callback_data) {
    (void)0;
}

void ffi_wallet_engine_rust_future_cancel_i32(uint64_t handle) {
    (void)0;
}

void ffi_wallet_engine_rust_future_free_i32(uint64_t handle) {
    (void)0;
}

int32_t ffi_wallet_engine_rust_future_complete_i32(uint64_t handle, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return 0;
}

void ffi_wallet_engine_rust_future_poll_u64(uint64_t handle, void * callback, uint64_t callback_data) {
    (void)0;
}

void ffi_wallet_engine_rust_future_cancel_u64(uint64_t handle) {
    (void)0;
}

void ffi_wallet_engine_rust_future_free_u64(uint64_t handle) {
    (void)0;
}

uint64_t ffi_wallet_engine_rust_future_complete_u64(uint64_t handle, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return 0;
}

void ffi_wallet_engine_rust_future_poll_i64(uint64_t handle, void * callback, uint64_t callback_data) {
    (void)0;
}

void ffi_wallet_engine_rust_future_cancel_i64(uint64_t handle) {
    (void)0;
}

void ffi_wallet_engine_rust_future_free_i64(uint64_t handle) {
    (void)0;
}

int64_t ffi_wallet_engine_rust_future_complete_i64(uint64_t handle, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return 0;
}

void ffi_wallet_engine_rust_future_poll_f32(uint64_t handle, void * callback, uint64_t callback_data) {
    (void)0;
}

void ffi_wallet_engine_rust_future_cancel_f32(uint64_t handle) {
    (void)0;
}

void ffi_wallet_engine_rust_future_free_f32(uint64_t handle) {
    (void)0;
}

float ffi_wallet_engine_rust_future_complete_f32(uint64_t handle, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return 0;
}

void ffi_wallet_engine_rust_future_poll_f64(uint64_t handle, void * callback, uint64_t callback_data) {
    (void)0;
}

void ffi_wallet_engine_rust_future_cancel_f64(uint64_t handle) {
    (void)0;
}

void ffi_wallet_engine_rust_future_free_f64(uint64_t handle) {
    (void)0;
}

double ffi_wallet_engine_rust_future_complete_f64(uint64_t handle, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return 0;
}

void ffi_wallet_engine_rust_future_poll_rust_buffer(uint64_t handle, void * callback, uint64_t callback_data) {
    (void)0;
}

void ffi_wallet_engine_rust_future_cancel_rust_buffer(uint64_t handle) {
    (void)0;
}

void ffi_wallet_engine_rust_future_free_rust_buffer(uint64_t handle) {
    (void)0;
}

RustBuffer ffi_wallet_engine_rust_future_complete_rust_buffer(uint64_t handle, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
    return RustBuffer{};
}

void ffi_wallet_engine_rust_future_poll_void(uint64_t handle, void * callback, uint64_t callback_data) {
    (void)0;
}

void ffi_wallet_engine_rust_future_cancel_void(uint64_t handle) {
    (void)0;
}

void ffi_wallet_engine_rust_future_free_void(uint64_t handle) {
    (void)0;
}

void ffi_wallet_engine_rust_future_complete_void(uint64_t handle, RustCallStatus *out_status) {
    if (out_status) {
        out_status->code = 1;
        out_status->error_buf = RustBuffer{};
    }
}

uint16_t uniffi_wallet_engine_checksum_func_parse_ton_transfer_link() {
    return 38144;
}

uint16_t uniffi_wallet_engine_checksum_func_parse_ton_connect_manifest() {
    return 55166;
}

uint16_t uniffi_wallet_engine_checksum_func_ton_connect_session_from_link() {
    return 16316;
}

uint16_t uniffi_wallet_engine_checksum_func_ton_connect_session_restore() {
    return 63276;
}

uint16_t uniffi_wallet_engine_checksum_func_convert_ton_address() {
    return 28540;
}

uint16_t uniffi_wallet_engine_checksum_func_is_valid_ton_address() {
    return 32896;
}

uint16_t uniffi_wallet_engine_checksum_func_parse_ton_address() {
    return 26131;
}

uint16_t uniffi_wallet_engine_checksum_func_mnemonic_wordlist() {
    return 1718;
}

uint16_t uniffi_wallet_engine_checksum_func_detect_mnemonic_schemes() {
    return 22911;
}

uint16_t uniffi_wallet_engine_checksum_func_rotation_mnemonic_public_key() {
    return 40177;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_cancel_load_more_activity() {
    return 30855;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_load_more_activity() {
    return 1728;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_shutdown() {
    return 12846;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_snapshot() {
    return 9252;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_wait_for_change() {
    return 21890;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_resolve_dns() {
    return 63449;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_create_encrypted_comment() {
    return 45205;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_decrypt_comment() {
    return 56301;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_resolve_encrypted_comment_recipient() {
    return 48846;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_prepare_key_rotation() {
    return 48426;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_cancel_load_more_nfts() {
    return 55435;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_cancel_refresh_nfts() {
    return 10319;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_load_more_nfts() {
    return 53428;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_refresh_nfts() {
    return 19017;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_preview_nft_transfer() {
    return 55191;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_send_nft_transfer() {
    return 6467;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_prepare_transfer() {
    return 35005;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_cancel_send_preview() {
    return 28394;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_preview_send() {
    return 55971;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_preview_send_boc() {
    return 21004;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_preview_sign_message() {
    return 5984;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_preview_ton_connect() {
    return 9616;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_cancel_refresh() {
    return 16959;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_refresh() {
    return 44556;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_resolve_pending() {
    return 41815;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_cancel_send() {
    return 37753;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_send() {
    return 23821;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_send_boc() {
    return 4063;
}

uint16_t uniffi_wallet_engine_checksum_method_walletclient_sign_message() {
    return 31537;
}

uint16_t uniffi_wallet_engine_checksum_method_walletplatformhost_read_protected_secret() {
    return 7657;
}

uint16_t uniffi_wallet_engine_checksum_method_walletplatformhost_store_protected_secret() {
    return 21396;
}

uint16_t uniffi_wallet_engine_checksum_method_walletplatformhost_delete_protected_secret() {
    return 43335;
}

uint16_t uniffi_wallet_engine_checksum_method_walletplatformhost_load_journal() {
    return 51398;
}

uint16_t uniffi_wallet_engine_checksum_method_walletplatformhost_compare_exchange_journal() {
    return 63993;
}

uint16_t uniffi_wallet_engine_checksum_method_tonconnectsession_approve_connect() {
    return 6077;
}

uint16_t uniffi_wallet_engine_checksum_method_tonconnectsession_begin_events_subscription() {
    return 62684;
}

uint16_t uniffi_wallet_engine_checksum_method_tonconnectsession_complete_pending_post() {
    return 52225;
}

uint16_t uniffi_wallet_engine_checksum_method_tonconnectsession_connect_prompt() {
    return 24215;
}

uint16_t uniffi_wallet_engine_checksum_method_tonconnectsession_disconnect() {
    return 14920;
}

uint16_t uniffi_wallet_engine_checksum_method_tonconnectsession_ingest_sse_chunk() {
    return 62248;
}

uint16_t uniffi_wallet_engine_checksum_method_tonconnectsession_pending_post() {
    return 12209;
}

uint16_t uniffi_wallet_engine_checksum_method_tonconnectsession_pending_requests() {
    return 22341;
}

uint16_t uniffi_wallet_engine_checksum_method_tonconnectsession_persisted() {
    return 36457;
}

uint16_t uniffi_wallet_engine_checksum_method_tonconnectsession_phase() {
    return 52857;
}

uint16_t uniffi_wallet_engine_checksum_method_tonconnectsession_prepare_disconnect_success() {
    return 65002;
}

uint16_t uniffi_wallet_engine_checksum_method_tonconnectsession_prepare_error() {
    return 22566;
}

uint16_t uniffi_wallet_engine_checksum_method_tonconnectsession_prepare_send_success() {
    return 31724;
}

uint16_t uniffi_wallet_engine_checksum_method_tonconnectsession_prepare_sign_message_success() {
    return 63431;
}

uint16_t uniffi_wallet_engine_checksum_method_tonconnectsession_reject_connect() {
    return 45812;
}

uint16_t uniffi_wallet_engine_checksum_method_tonconnectderivedsession_decode_sign_data_cell() {
    return 49244;
}

uint16_t uniffi_wallet_engine_checksum_method_tonconnectderivedsession_decrypt_request() {
    return 40213;
}

uint16_t uniffi_wallet_engine_checksum_method_tonconnectderivedsession_encrypt_connect_error() {
    return 3125;
}

uint16_t uniffi_wallet_engine_checksum_method_tonconnectderivedsession_encrypt_connect_event() {
    return 12874;
}

uint16_t uniffi_wallet_engine_checksum_method_tonconnectderivedsession_encrypt_disconnect_event() {
    return 15862;
}

uint16_t uniffi_wallet_engine_checksum_method_tonconnectderivedsession_encrypt_disconnect_success() {
    return 11931;
}

uint16_t uniffi_wallet_engine_checksum_method_tonconnectderivedsession_encrypt_error() {
    return 24657;
}

uint16_t uniffi_wallet_engine_checksum_method_tonconnectderivedsession_encrypt_send_success() {
    return 2099;
}

uint16_t uniffi_wallet_engine_checksum_method_tonconnectderivedsession_encrypt_sign_data_success() {
    return 48287;
}

uint16_t uniffi_wallet_engine_checksum_method_tonconnectderivedsession_open_challenge() {
    return 24298;
}

uint16_t uniffi_wallet_engine_checksum_method_tonconnectderivedsession_public_key_hex() {
    return 32021;
}

uint16_t uniffi_wallet_engine_checksum_method_tonconnectderivedsession_signing_public_key() {
    return 11397;
}

uint16_t uniffi_wallet_engine_checksum_method_wallethttphost_execute_http() {
    return 43126;
}

uint16_t uniffi_wallet_engine_checksum_method_wallethttphost_cancel_http() {
    return 55221;
}

uint16_t uniffi_wallet_engine_checksum_method_walletstatuslesshost_execute_statusless() {
    return 29141;
}

uint16_t uniffi_wallet_engine_checksum_method_walletstatuslesshost_cancel_statusless() {
    return 58537;
}

uint16_t uniffi_wallet_engine_checksum_method_walletlifecycle_create_wallet() {
    return 58283;
}

uint16_t uniffi_wallet_engine_checksum_method_walletlifecycle_delete_wallet() {
    return 60089;
}

uint16_t uniffi_wallet_engine_checksum_method_walletlifecycle_derive_ton_connect_session() {
    return 2602;
}

uint16_t uniffi_wallet_engine_checksum_method_walletlifecycle_import_wallet() {
    return 60352;
}

uint16_t uniffi_wallet_engine_checksum_method_walletlifecycle_reveal_recovery_phrase() {
    return 7144;
}

uint16_t uniffi_wallet_engine_checksum_method_walletlifecycle_sign_ton_connect_data() {
    return 38868;
}

uint16_t uniffi_wallet_engine_checksum_method_walletlifecycle_sign_ton_connect_proof() {
    return 59978;
}

uint16_t uniffi_wallet_engine_checksum_method_walletlifecycle_ton_connect_account() {
    return 32302;
}

uint16_t uniffi_wallet_engine_checksum_constructor_walletclient_new() {
    return 8434;
}

uint16_t uniffi_wallet_engine_checksum_constructor_walletclient_new_statusless() {
    return 59466;
}

uint16_t uniffi_wallet_engine_checksum_constructor_walletlifecycle_new() {
    return 42116;
}

uint32_t ffi_wallet_engine_uniffi_contract_version() {
    return 30;
}

} // extern "C"
