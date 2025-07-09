# Copyright (c) 2018-2021 The Fittexxcoin developers

project(fittexxcoin-qt-locale)

include(BrewHelper)
find_brew_prefix(QT5_PREFIX qt5)

find_package(Qt5LinguistTools HINTS "${QT5_PREFIX}")

if(Qt5LinguistTools_VERSION VERSION_GREATER_EQUAL 5.11)
	set(QT5_ADD_TRANSLATION_OPTIONS OPTIONS -silent)
endif()

qt5_add_translation(FITTEXXCOIN_QM_FILES
	fittexxcoin_af.ts
	fittexxcoin_af_ZA.ts
	fittexxcoin_ar.ts
	fittexxcoin_be_BY.ts
	fittexxcoin_bg_BG.ts
	fittexxcoin_bg.ts
	fittexxcoin_ca_ES.ts
	fittexxcoin_ca.ts
	fittexxcoin_ca@valencia.ts
	fittexxcoin_cs.ts
	fittexxcoin_cy.ts
	fittexxcoin_da.ts
	fittexxcoin_de.ts
	fittexxcoin_el_GR.ts
	fittexxcoin_el.ts
	fittexxcoin_en_GB.ts
	fittexxcoin_en.ts
	fittexxcoin_eo.ts
	fittexxcoin_es_AR.ts
	fittexxcoin_es_CL.ts
	fittexxcoin_es_CO.ts
	fittexxcoin_es_DO.ts
	fittexxcoin_es_ES.ts
	fittexxcoin_es_MX.ts
	fittexxcoin_es.ts
	fittexxcoin_es_UY.ts
	fittexxcoin_es_VE.ts
	fittexxcoin_et_EE.ts
	fittexxcoin_et.ts
	fittexxcoin_eu_ES.ts
	fittexxcoin_fa_IR.ts
	fittexxcoin_fa.ts
	fittexxcoin_fi.ts
	fittexxcoin_fr_CA.ts
	fittexxcoin_fr_FR.ts
	fittexxcoin_fr.ts
	fittexxcoin_gl.ts
	fittexxcoin_he.ts
	fittexxcoin_hi_IN.ts
	fittexxcoin_hr.ts
	fittexxcoin_hu.ts
	fittexxcoin_id_ID.ts
	fittexxcoin_it.ts
	fittexxcoin_ja.ts
	fittexxcoin_ka.ts
	fittexxcoin_kk_KZ.ts
	fittexxcoin_ko_KR.ts
	fittexxcoin_ku_IQ.ts
	fittexxcoin_ky.ts
	fittexxcoin_la.ts
	fittexxcoin_lt.ts
	fittexxcoin_lv_LV.ts
	fittexxcoin_mk_MK.ts
	fittexxcoin_mn.ts
	fittexxcoin_ms_MY.ts
	fittexxcoin_nb.ts
	fittexxcoin_ne.ts
	fittexxcoin_nl.ts
	fittexxcoin_pam.ts
	fittexxcoin_pl.ts
	fittexxcoin_pt_BR.ts
	fittexxcoin_pt_PT.ts
	fittexxcoin_ro_RO.ts
	fittexxcoin_ro.ts
	fittexxcoin_ru_RU.ts
	fittexxcoin_ru.ts
	fittexxcoin_sk.ts
	fittexxcoin_sl_SI.ts
	fittexxcoin_sq.ts
	fittexxcoin_sr@latin.ts
	fittexxcoin_sr.ts
	fittexxcoin_sv.ts
	fittexxcoin_sw.ts
	fittexxcoin_ta.ts
	fittexxcoin_th_TH.ts
	fittexxcoin_tr_TR.ts
	fittexxcoin_tr.ts
	fittexxcoin_uk.ts
	fittexxcoin_ur_PK.ts
	fittexxcoin_uz@Cyrl.ts
	fittexxcoin_vi.ts
	fittexxcoin_vi_VN.ts
	fittexxcoin_zh_HK.ts
	fittexxcoin_zh.ts
	fittexxcoin_zh_TW.ts
	${QT5_ADD_TRANSLATION_OPTIONS}
)

add_custom_target(locales DEPENDS ${FITTEXXCOIN_QM_FILES})
