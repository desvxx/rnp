/*
 * Copyright (c) 2017-2023, [Ribose Inc](https://www.ribose.com).
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is originally derived from software contributed to
 * The NetBSD Foundation by Alistair Crooks (agc@netbsd.org), and
 * carried further by Ribose Inc (https://www.ribose.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 2005-2008 Nominet UK (www.nic.uk)
 * All rights reserved.
 * Contributors: Ben Laurie, Rachel Willmer. The Contributors have asserted
 * their moral rights under the UK Copyright Design and Patents Act 1988 to
 * be recorded as the authors of this copyright work.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "config.h"

#ifdef HAVE_SYS_CDEFS_H
#include <sys/cdefs.h>
#endif

#if defined(__NetBSD__)
__COPYRIGHT("@(#) Copyright (c) 2009 The NetBSD Foundation, Inc. All rights reserved.");
__RCSID("$NetBSD: crypto.c,v 1.36 2014/02/17 07:39:19 agc Exp $");
#endif

#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <string.h>
#include <time.h>
#include <rnp/rnp_def.h>

#include <librepgp/stream-packet.h>
#include <librepgp/stream-key.h>

#include "types.h"
#include "crypto/common.h"
#include "crypto.h"
#include "fingerprint.h"
#include "pgp-key.h"
#include "utils.h"

bool
pgp_generate_seckey(const rnp_keygen_crypto_params_t &crypto,
                    pgp_key_pkt_t &                   seckey,
                    bool                              primary,
                    pgp_version_t                     pgp_version)
{
    /* populate pgp key structure */
    seckey = {};
#if defined(ENABLE_CRYPTO_REFRESH)
    seckey.version = pgp_version;
#else
    seckey.version = PGP_V4;
#endif
    seckey.creation_time = crypto.ctx->time();
    seckey.alg = crypto.key_alg;
    seckey.material.alg = crypto.key_alg;
    seckey.tag = primary ? PGP_PKT_SECRET_KEY : PGP_PKT_SECRET_SUBKEY;

    switch (seckey.alg) {
    case PGP_PKA_RSA:
        if (rsa_generate(&crypto.ctx->rng, &seckey.material.rsa, crypto.rsa.modulus_bit_len)) {
            RNP_LOG("failed to generate RSA key");
            return false;
        }
        break;
    case PGP_PKA_DSA:
        if (dsa_generate(&crypto.ctx->rng,
                         &seckey.material.dsa,
                         crypto.dsa.p_bitlen,
                         crypto.dsa.q_bitlen)) {
            RNP_LOG("failed to generate DSA key");
            return false;
        }
        break;
    case PGP_PKA_EDDSA:
        if (eddsa_generate(&crypto.ctx->rng, &seckey.material.ec)) {
            RNP_LOG("failed to generate EDDSA key");
            return false;
        }
        break;
    case PGP_PKA_ECDH:
        if (!ecdh_set_params(&seckey.material.ec, crypto.ecc.curve)) {
            RNP_LOG("Unsupported curve [ID=%d]", crypto.ecc.curve);
            return false;
        }
        if (crypto.ecc.curve == PGP_CURVE_25519) {
            if (x25519_generate(&crypto.ctx->rng, &seckey.material.ec)) {
                RNP_LOG("failed to generate x25519 key");
                return false;
            }
            seckey.material.ec.curve = crypto.ecc.curve;
            break;
        }
        FALLTHROUGH_STATEMENT;
    case PGP_PKA_ECDSA:
    case PGP_PKA_SM2:
        if (!curve_supported(crypto.ecc.curve)) {
            RNP_LOG("EC generate: curve %d is not supported.", (int) crypto.ecc.curve);
            return false;
        }
        if (ec_generate(&crypto.ctx->rng, &seckey.material.ec, seckey.alg, crypto.ecc.curve)) {
            RNP_LOG("failed to generate EC key");
            return false;
        }
        seckey.material.ec.curve = crypto.ecc.curve;
        break;
    case PGP_PKA_ELGAMAL:
        if (elgamal_generate(
              &crypto.ctx->rng, &seckey.material.eg, crypto.elgamal.key_bitlen)) {
            RNP_LOG("failed to generate ElGamal key");
            return false;
        }
        break;
#if defined(ENABLE_CRYPTO_REFRESH)
    case PGP_PKA_ED25519:
        if (generate_ed25519_native(&crypto.ctx->rng,
                                    seckey.material.ed25519.priv,
                                    seckey.material.ed25519.pub) != RNP_SUCCESS) {
            RNP_LOG("failed to generate ED25519 key");
            return false;
        }
        break;
    case PGP_PKA_X25519:
        if (generate_x25519_native(&crypto.ctx->rng,
                                   seckey.material.x25519.priv,
                                   seckey.material.x25519.pub) != RNP_SUCCESS) {
            RNP_LOG("failed to generate X25519 key");
            return false;
        }
        break;
#endif
#if defined(ENABLE_PQC)
    case PGP_PKA_KYBER768_X25519:
        FALLTHROUGH_STATEMENT;
    // TODO add case PGP_PKA_KYBER1024_X448: FALLTHROUGH_STATEMENT;
    case PGP_PKA_KYBER768_P256:
        FALLTHROUGH_STATEMENT;
    case PGP_PKA_KYBER1024_P384:
        FALLTHROUGH_STATEMENT;
    case PGP_PKA_KYBER768_BP256:
        FALLTHROUGH_STATEMENT;
    case PGP_PKA_KYBER1024_BP384:
        if (pgp_kyber_ecdh_composite_key_t::gen_keypair(
              &crypto.ctx->rng, &seckey.material.kyber_ecdh, seckey.alg)) {
            RNP_LOG("failed to generate MLKEM-ECDH-composite key for PK alg %d", seckey.alg);
            return false;
        }
        break;
    case PGP_PKA_DILITHIUM3_ED25519:
        FALLTHROUGH_STATEMENT;
    // TODO: add case PGP_PKA_DILITHIUM5_ED448: FALLTHROUGH_STATEMENT;
    case PGP_PKA_DILITHIUM3_P256:
        FALLTHROUGH_STATEMENT;
    case PGP_PKA_DILITHIUM5_P384:
        FALLTHROUGH_STATEMENT;
    case PGP_PKA_DILITHIUM3_BP256:
        FALLTHROUGH_STATEMENT;
    case PGP_PKA_DILITHIUM5_BP384:
        if (pgp_dilithium_exdsa_composite_key_t::gen_keypair(
              &crypto.ctx->rng, &seckey.material.dilithium_exdsa, seckey.alg)) {
            RNP_LOG("failed to generate mldsa-ecdsa/eddsa-composite key for PK alg %d",
                    seckey.alg);
            return false;
        }
        break;
    case PGP_PKA_SPHINCSPLUS_SHA2:
        FALLTHROUGH_STATEMENT;
    case PGP_PKA_SPHINCSPLUS_SHAKE:
        if (pgp_sphincsplus_generate(&crypto.ctx->rng,
                                     &seckey.material.sphincsplus,
                                     crypto.sphincsplus.param,
                                     seckey.alg)) {
            RNP_LOG("failed to generate SLH-DSA key for PK alg %d", seckey.alg);
            return false;
        }
        break;
#endif
    default:
        RNP_LOG("key generation not implemented for PK alg: %d", seckey.alg);
        return false;
    }
    seckey.sec_protection.s2k.usage = PGP_S2KU_NONE;
    seckey.material.secret = true;
    seckey.material.validity.mark_valid();
    /* fill the sec_data/sec_len */
    if (encrypt_secret_key(&seckey, NULL, crypto.ctx->rng)) {
        RNP_LOG("failed to fill sec_data");
        return false;
    }
    return true;
}

bool
key_material_equal(const pgp_key_material_t *key1, const pgp_key_material_t *key2)
{
    if (key1->alg != key2->alg) {
        return false;
    }

    switch (key1->alg) {
    case PGP_PKA_RSA:
    case PGP_PKA_RSA_ENCRYPT_ONLY:
    case PGP_PKA_RSA_SIGN_ONLY:
        return (key1->rsa.n == key2->rsa.n) && (key1->rsa.e == key2->rsa.e);
    case PGP_PKA_DSA:
        return (key1->dsa.p == key2->dsa.p) && (key1->dsa.q == key2->dsa.q) &&
               (key1->dsa.g == key2->dsa.g) && (key1->dsa.y == key2->dsa.y);
    case PGP_PKA_ELGAMAL:
    case PGP_PKA_ELGAMAL_ENCRYPT_OR_SIGN:
        return (key1->eg.p == key2->eg.p) && (key1->eg.g == key2->eg.g) &&
               (key1->eg.y == key2->eg.y);
    case PGP_PKA_EDDSA:
    case PGP_PKA_ECDH:
    case PGP_PKA_ECDSA:
    case PGP_PKA_SM2:
        return (key1->ec.curve == key2->ec.curve) && (key1->ec.p == key2->ec.p);
#if defined(ENABLE_CRYPTO_REFRESH)
    case PGP_PKA_ED25519:
        return (key1->ed25519.pub == key2->ed25519.pub);
    case PGP_PKA_X25519:
        return (key1->x25519.pub == key2->x25519.pub);
#endif
#if defined(ENABLE_PQC)
    case PGP_PKA_KYBER768_X25519:
        FALLTHROUGH_STATEMENT;
    // TODO add case PGP_PKA_KYBER1024_X448: FALLTHROUGH_STATEMENT;
    case PGP_PKA_KYBER768_P256:
        FALLTHROUGH_STATEMENT;
    case PGP_PKA_KYBER1024_P384:
        FALLTHROUGH_STATEMENT;
    case PGP_PKA_KYBER768_BP256:
        FALLTHROUGH_STATEMENT;
    case PGP_PKA_KYBER1024_BP384:
        return (key1->kyber_ecdh.pub == key2->kyber_ecdh.pub);
    case PGP_PKA_DILITHIUM3_ED25519:
        FALLTHROUGH_STATEMENT;
    // TODO: add case PGP_PKA_DILITHIUM5_ED448: FALLTHROUGH_STATEMENT;
    case PGP_PKA_DILITHIUM3_P256:
        FALLTHROUGH_STATEMENT;
    case PGP_PKA_DILITHIUM5_P384:
        FALLTHROUGH_STATEMENT;
    case PGP_PKA_DILITHIUM3_BP256:
        FALLTHROUGH_STATEMENT;
    case PGP_PKA_DILITHIUM5_BP384:
        return (key1->dilithium_exdsa.pub == key2->dilithium_exdsa.pub);
    case PGP_PKA_SPHINCSPLUS_SHA2:
        FALLTHROUGH_STATEMENT;
    case PGP_PKA_SPHINCSPLUS_SHAKE:
        return (key1->sphincsplus.pub == key2->sphincsplus.pub);
#endif
    default:
        RNP_LOG("unknown public key algorithm: %d", (int) key1->alg);
        return false;
    }
}

rnp_result_t
validate_pgp_key_material(const pgp_key_material_t *material, rnp::RNG *rng)
{
#ifdef FUZZERS_ENABLED
    /* do not timeout on large keys during fuzzing */
    return RNP_SUCCESS;
#else
    switch (material->alg) {
    case PGP_PKA_RSA:
    case PGP_PKA_RSA_ENCRYPT_ONLY:
    case PGP_PKA_RSA_SIGN_ONLY:
        return rsa_validate_key(rng, &material->rsa, material->secret);
    case PGP_PKA_DSA:
        return dsa_validate_key(rng, &material->dsa, material->secret);
    case PGP_PKA_EDDSA:
        return eddsa_validate_key(rng, &material->ec, material->secret);
    case PGP_PKA_ECDH:
        if (!curve_supported(material->ec.curve)) {
            /* allow to import key if curve is not supported */
            RNP_LOG("ECDH validate: curve %d is not supported.", (int) material->ec.curve);
            return RNP_SUCCESS;
        }
        return ecdh_validate_key(rng, &material->ec, material->secret);
    case PGP_PKA_ECDSA:
        if (!curve_supported(material->ec.curve)) {
            /* allow to import key if curve is not supported */
            RNP_LOG("ECDH validate: curve %d is not supported.", (int) material->ec.curve);
            return RNP_SUCCESS;
        }
        return ecdsa_validate_key(rng, &material->ec, material->secret);
    case PGP_PKA_SM2:
#if defined(ENABLE_SM2)
        return sm2_validate_key(rng, &material->ec, material->secret);
#else
        RNP_LOG("SM2 key validation is not available.");
        return RNP_ERROR_NOT_IMPLEMENTED;
#endif
    case PGP_PKA_ELGAMAL:
    case PGP_PKA_ELGAMAL_ENCRYPT_OR_SIGN:
        return elgamal_validate_key(&material->eg, material->secret) ? RNP_SUCCESS :
                                                                       RNP_ERROR_GENERIC;
#if defined(ENABLE_CRYPTO_REFRESH)
    case PGP_PKA_ED25519:
        return ed25519_validate_key_native(rng, &material->ed25519, material->secret);
    case PGP_PKA_X25519:
        return x25519_validate_key_native(rng, &material->x25519, material->secret);
#endif
#if defined(ENABLE_PQC)
    case PGP_PKA_KYBER768_X25519:
        FALLTHROUGH_STATEMENT;
    // TODO add case PGP_PKA_KYBER1024_X448: FALLTHROUGH_STATEMENT;
    case PGP_PKA_KYBER768_P256:
        FALLTHROUGH_STATEMENT;
    case PGP_PKA_KYBER1024_P384:
        FALLTHROUGH_STATEMENT;
    case PGP_PKA_KYBER768_BP256:
        FALLTHROUGH_STATEMENT;
    case PGP_PKA_KYBER1024_BP384:
        return kyber_ecdh_validate_key(rng, &material->kyber_ecdh, material->secret);
    case PGP_PKA_DILITHIUM3_ED25519:
        FALLTHROUGH_STATEMENT;
    // TODO: add case PGP_PKA_DILITHIUM5_ED448: FALLTHROUGH_STATEMENT;
    case PGP_PKA_DILITHIUM3_P256:
        FALLTHROUGH_STATEMENT;
    case PGP_PKA_DILITHIUM5_P384:
        FALLTHROUGH_STATEMENT;
    case PGP_PKA_DILITHIUM3_BP256:
        FALLTHROUGH_STATEMENT;
    case PGP_PKA_DILITHIUM5_BP384:
        return dilithium_exdsa_validate_key(rng, &material->dilithium_exdsa, material->secret);
    case PGP_PKA_SPHINCSPLUS_SHA2:
        FALLTHROUGH_STATEMENT;
    case PGP_PKA_SPHINCSPLUS_SHAKE:
        return sphincsplus_validate_key(rng, &material->sphincsplus, material->secret);
#endif
    default:
        RNP_LOG("unknown public key algorithm: %d", (int) material->alg);
    }

    return RNP_ERROR_BAD_PARAMETERS;
#endif
}
