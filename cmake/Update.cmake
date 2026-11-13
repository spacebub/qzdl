# cmake [-DYES=ON] [-DMAJOR=ON] -P cmake/Update.cmake
# YES skips the confirmation. MAJOR also takes updates outside the pinned series.
cmake_minimum_required(VERSION 3.25)

cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH root)
cmake_path(GET root FILENAME project)
find_program(git git REQUIRED)

if (DEFINED ENV{TMPDIR})
    file(TO_CMAKE_PATH "$ENV{TMPDIR}" scratch)
elseif (DEFINED ENV{TEMP})
    file(TO_CMAKE_PATH "$ENV{TEMP}" scratch)
else ()
    set(scratch /tmp)
endif ()

set(scratch "${scratch}/${project}-update")
file(REMOVE_RECURSE "${scratch}")
file(MAKE_DIRECTORY "${scratch}")

function(version_of tag out)
    string(REGEX MATCHALL "[0-9]+" numbers "${tag}")
    list(JOIN numbers "." version)
    set(${out} "${version}" PARENT_SCOPE)
endfunction()

# Cargo rules: versions are compatible while they agree up to and including the first number that is not zero.
function(series_of version out)
    string(REPLACE "." ";" numbers "${version}")
    set(series "")

    foreach (number IN LISTS numbers)
        list(APPEND series ${number})

        if (NOT number EQUAL 0)
            break()
        endif ()
    endforeach ()

    list(JOIN series "." series)
    set(${out} "${series}" PARENT_SCOPE)
endfunction()

# Only tags shaped like the pinned one count, which leaves out release candidates and snapshots.
function(newest_tags repo current compatible major)
    set(${compatible} "" PARENT_SCOPE)
    set(${major} "" PARENT_SCOPE)

    execute_process(COMMAND ${git} ls-remote --tags --refs ${repo}
            OUTPUT_VARIABLE refs RESULT_VARIABLE result ERROR_QUIET)

    if (NOT result EQUAL 0)
        message(WARNING "Could not list the tags of ${repo}")
        return()
    endif ()

    string(REGEX MATCHALL "refs/tags/[^\n]+" refs "${refs}")
    string(REGEX REPLACE "[0-9]+" "#" shape "${current}")
    version_of("${current}" pinned)
    series_of("${pinned}" pinned_series)
    set(best_compatible "${pinned}")
    set(best_major "${pinned}")

    foreach (ref IN LISTS refs)
        string(SUBSTRING "${ref}" 10 -1 tag)
        string(REGEX REPLACE "[0-9]+" "#" tag_shape "${tag}")

        if (NOT tag_shape STREQUAL shape)
            continue()
        endif ()

        version_of("${tag}" version)
        series_of("${version}" series)

        if (series STREQUAL pinned_series AND version VERSION_GREATER best_compatible)
            set(best_compatible "${version}")
            set(${compatible} "${tag}" PARENT_SCOPE)
        elseif (NOT series STREQUAL pinned_series AND version VERSION_GREATER best_major)
            set(best_major "${version}")
            set(${major} "${tag}" PARENT_SCOPE)
        endif ()
    endforeach ()
endfunction()

function(pick compatible major out)
    if (MAJOR AND major)
        set(${out} "${major}" PARENT_SCOPE)
    else ()
        set(${out} "${compatible}" PARENT_SCOPE)
    endif ()
endfunction()

# The part of a tag that ${variable} stands for in the template, such as 3.6.7 out of mbedtls-3.6.7.
function(strip_template template variable tag out)
    if (tag STREQUAL "")
        set(${out} "" PARENT_SCOPE)
        return()
    endif ()

    string(FIND "${template}" "\${${variable}}" at)
    string(LENGTH "\${${variable}}" length)
    math(EXPR after "${at} + ${length}")
    string(SUBSTRING "${template}" ${after} -1 suffix)
    string(LENGTH "${suffix}" suffix_length)
    string(LENGTH "${tag}" tag_length)
    math(EXPR length "${tag_length} - ${at} - ${suffix_length}")
    string(SUBSTRING "${tag}" ${at} ${length} value)
    set(${out} "${value}" PARENT_SCOPE)
endfunction()

function(hash_of url algorithm out)
    message("Hashing ${url}")
    file(DOWNLOAD ${url} "${scratch}/archive" STATUS status)
    list(GET status 0 code)

    if (NOT code EQUAL 0)
        message(FATAL_ERROR "Could not download ${url}: ${status}")
    endif ()

    file(${algorithm} "${scratch}/archive" hash)
    file(REMOVE "${scratch}/archive")
    set(${out} "${hash}" PARENT_SCOPE)
endfunction()

function(head_of repo ref out)
    execute_process(COMMAND ${git} ls-remote ${repo} ${ref} OUTPUT_VARIABLE head RESULT_VARIABLE result ERROR_QUIET)

    if (NOT result EQUAL 0 OR head STREQUAL "")
        message(FATAL_ERROR "Could not read ${ref} of ${repo}")
    endif ()

    string(SUBSTRING "${head}" 0 40 head)
    set(${out} "${head}" PARENT_SCOPE)
endfunction()

# Each offer is a row of the overview, then the edits that apply it as triples of file, old text and new text.
# A new text of "hash <algorithm> <url>" is downloaded and hashed only once the updates are confirmed.
set(offers 0)
set(updates "")
set(majors "")
set(commits "")
set(unchecked "")

function(offer list name old new where)
    math(EXPR index "${offers} + 1")
    set(offers ${index} PARENT_SCOPE)
    set(${list} ${${list}} ${index} PARENT_SCOPE)
    set(offer_${index}_row "${name};${old};${new};${where}" PARENT_SCOPE)
    set(offer_${index}_edits "${ARGN}" PARENT_SCOPE)
endfunction()

message("Checking the dependencies of ${project}")

execute_process(COMMAND ${git} -C ${root} ls-files -- "*CMakeLists.txt" "*.cmake"
        OUTPUT_VARIABLE files OUTPUT_STRIP_TRAILING_WHITESPACE)
string(REPLACE "\n" ";" files "${files}")
set(everything "")

foreach (path IN LISTS files)
    file(READ "${root}/${path}" text)
    string(APPEND everything "${text}")
    string(REGEX MATCHALL "set\\([A-Za-z0-9_]+[ \t]+[^ \t\n)]+\\)" lines "${text}")

    foreach (line IN LISTS lines)
        string(REGEX MATCH "^set\\(([A-Za-z0-9_]+)[ \t]+(.+)\\)$" _ "${line}")
        set(pin_${CMAKE_MATCH_1}_file "${path}")
        set(pin_${CMAKE_MATCH_1}_line "${line}")
        set(pin_${CMAKE_MATCH_1}_value "${CMAKE_MATCH_2}")
    endforeach ()
endforeach ()

foreach (path IN LISTS files)
    file(READ "${root}/${path}" text)
    string(REGEX MATCHALL "FetchContent_Declare\\([^)]*\\)" blocks "${text}")

    foreach (block IN LISTS blocks)
        string(REGEX MATCH "^FetchContent_Declare\\(([A-Za-z0-9_]+)" _ "${block}")
        set(name "${CMAKE_MATCH_1}")

        set(variable "")

        if (block MATCHES "\\$\\{([A-Za-z0-9_]+_VERSION)\\}")
            set(variable "${CMAKE_MATCH_1}")
        endif ()

        if (NOT DEFINED pin_${variable}_value)
            list(APPEND unchecked ${name})
            continue()
        endif ()

        set(value "${pin_${variable}_value}")
        set(line "${pin_${variable}_line}")
        set(where "${pin_${variable}_file}")

        if (everything MATCHES "_patch_source\\(${name}[ \t\n]")
            set(shown "${where}, patched")
        else ()
            set(shown "${where}")
        endif ()

        set(repo "")

        if (block MATCHES "GIT_REPOSITORY[ \t\n]+([^ \t\n)]+)")
            set(repo "${CMAKE_MATCH_1}")
        endif ()

        string(LENGTH "${value}" length)
        set(commit OFF)

        if (length EQUAL 40 AND value MATCHES "^[0-9a-f]+$")
            set(commit ON)
        endif ()

        if (repo AND commit)
            head_of(${repo} HEAD head)
            string(SUBSTRING "${value}" 0 8 short)
            string(SUBSTRING "${head}" 0 8 head_short)

            if (head STREQUAL value)
                list(APPEND commits ${name} "${short} is the upstream head")
            else ()
                list(APPEND commits ${name} "${short}, upstream head is ${head_short}")
            endif ()
        elseif (repo)
            newest_tags(${repo} ${value} compatible major)
            pick("${compatible}" "${major}" new)

            if (new)
                offer(updates ${name} ${value} ${new} "${shown}" "${where}" "${line}" "set(${variable} ${new})")
            endif ()

            if (major AND NOT MAJOR)
                offer(majors ${name} ${value} ${major} "${shown}")
            endif ()
        elseif (NOT block MATCHES "[ \t\n]URL[ \t\n]+([^ \t\n)]+)")
            list(APPEND unchecked ${name})
        else ()
            set(url "${CMAKE_MATCH_1}")

            if (NOT block MATCHES "URL_HASH[ \t\n]+([A-Z0-9_]+)=([^ \t\n)]+)")
                list(APPEND unchecked ${name})
                continue()
            endif ()

            set(algorithm "${CMAKE_MATCH_1}")
            set(hash "${CMAKE_MATCH_2}")
            set(hash_file "${path}")

            if (hash MATCHES "^\\$\\{([A-Za-z0-9_]+)\\}$")
                set(hash_file "${pin_${CMAKE_MATCH_1}_file}")
                set(hash "${pin_${CMAKE_MATCH_1}_value}")
            endif ()

            if (NOT hash MATCHES "^[0-9a-fA-F]+$")
                list(APPEND unchecked ${name})
                continue()
            endif ()

            if (url MATCHES "^https://github\\.com/([^/]+/[^/]+)/releases/download/([^/]+)/")
                set(template "${CMAKE_MATCH_2}")
                string(REPLACE "\${${variable}}" "${value}" current "${template}")
                newest_tags(https://github.com/${CMAKE_MATCH_1}.git ${current} compatible major)
                strip_template("${template}" ${variable} "${compatible}" compatible)
                strip_template("${template}" ${variable} "${major}" major)
                pick("${compatible}" "${major}" new)

                if (new)
                    string(REPLACE "\${${variable}}" "${new}" new_url "${url}")
                    set(new_hash "hash ${algorithm} ${new_url}")
                    offer(updates ${name} ${value} ${new} "${shown}"
                            "${where}" "${line}" "set(${variable} ${new})" "${hash_file}" "${hash}" "${new_hash}")
                endif ()

                if (major AND NOT MAJOR)
                    offer(majors ${name} ${value} ${major} "${shown}")
                endif ()
            elseif (url MATCHES "^https://www\\.sqlite\\.org/([^/]+)/sqlite-amalgamation-")
                set(year_text "${CMAKE_MATCH_1}")
                set(year "${year_text}")
                set(year_variable "")

                if (year_text MATCHES "^\\$\\{([A-Za-z0-9_]+)\\}$")
                    set(year_variable "${CMAKE_MATCH_1}")
                    set(year "${pin_${year_variable}_value}")
                endif ()

                file(DOWNLOAD https://www.sqlite.org/download.html "${scratch}/sqlite.html")
                file(STRINGS "${scratch}/sqlite.html" products
                        REGEX "PRODUCT,[^,]*,[0-9]+/sqlite-amalgamation-[0-9]+\\.zip,")

                if (NOT products MATCHES "PRODUCT,([^,]*),([0-9]+)/sqlite-amalgamation-([0-9]+)\\.zip,[0-9]+,([0-9a-f]+)")
                    message(WARNING "Could not read the latest SQLite from its download page")
                    list(APPEND unchecked ${name})
                    continue()
                endif ()

                set(release "${CMAKE_MATCH_1}")
                set(new_year "${CMAKE_MATCH_2}")
                set(new "${CMAKE_MATCH_3}")
                set(new_hash "${CMAKE_MATCH_4}")

                if (new VERSION_GREATER value)
                    # The download page lists SHA3-256, so any other algorithm needs the archive itself.
                    if (NOT algorithm STREQUAL "SHA3_256")
                        string(REPLACE "\${${variable}}" "${new}" new_url "${url}")
                        string(REPLACE "/${year_text}/" "/${new_year}/" new_url "${new_url}")
                        set(new_hash "hash ${algorithm} ${new_url}")
                    endif ()

                    if (year_variable)
                        set(year_edit "${pin_${year_variable}_file}" "${pin_${year_variable}_line}"
                                "set(${year_variable} ${new_year})")
                    else ()
                        set(year_edit "${path}" "sqlite.org/${year}/" "sqlite.org/${new_year}/")
                    endif ()

                    offer(updates ${name} ${value} "${new} (${release})" "${shown}"
                            "${where}" "${line}" "set(${variable} ${new})" ${year_edit}
                            "${hash_file}" "${hash}" "${new_hash}")
                endif ()
            else ()
                list(APPEND unchecked ${name})
            endif ()
        endif ()
    endforeach ()
endforeach ()

set(overlays "")

if (EXISTS "${root}/vcpkg-configuration.json")
    file(READ "${root}/vcpkg-configuration.json" configuration)
    string(JSON count ERROR_VARIABLE missing LENGTH "${configuration}" overlay-ports)

    if (NOT missing AND count GREATER 0)
        math(EXPR last "${count} - 1")

        foreach (index RANGE ${last})
            string(JSON directory GET "${configuration}" overlay-ports ${index})
            file(GLOB ports LIST_DIRECTORIES true RELATIVE "${root}" "${root}/${directory}/*")

            foreach (port IN LISTS ports)
                if (EXISTS "${root}/${port}/portfile.cmake")
                    list(APPEND overlays ${port})
                endif ()
            endforeach ()
        endforeach ()
    endif ()
endif ()

set(overlay_names "")

# Only ports fetched with vcpkg_from_github at a REF built from their version can be followed.
foreach (port IN LISTS overlays)
    cmake_path(GET port FILENAME name)
    list(APPEND overlay_names ${name})
    file(READ "${root}/${port}/portfile.cmake" portfile)
    file(READ "${root}/${port}/vcpkg.json" manifest)
    string(JSON version ERROR_VARIABLE missing GET "${manifest}" version)

    if (missing OR NOT manifest MATCHES "\"version\"[ \t]*:[ \t]*\"${version}\"")
        list(APPEND unchecked ${name})
        continue()
    endif ()

    set(version_text "${CMAKE_MATCH_0}")

    if (NOT portfile MATCHES "vcpkg_from_github\\([^)]*REPO[ \t\n]+([^ \t\n)]+)[ \t\n]+REF[ \t\n]+([^ \t\n)]*\\$\\{VERSION\\}[^ \t\n)]*)[ \t\n]+SHA512[ \t\n]+([0-9a-f]+)")
        list(APPEND unchecked ${name})
        continue()
    endif ()

    set(repo "${CMAKE_MATCH_1}")
    set(ref "${CMAKE_MATCH_2}")
    set(hash "${CMAKE_MATCH_3}")
    string(REPLACE "\${VERSION}" "${version}" current "${ref}")
    newest_tags(https://github.com/${repo}.git ${current} compatible major)
    strip_template("${ref}" VERSION "${compatible}" compatible)
    strip_template("${ref}" VERSION "${major}" major)
    pick("${compatible}" "${major}" new)

    if (new)
        string(REPLACE "\${VERSION}" "${new}" new_ref "${ref}")
        set(new_hash "hash SHA512 https://github.com/${repo}/archive/${new_ref}.tar.gz")
        string(REPLACE "\"${version}\"" "\"${new}\"" new_version_text "${version_text}")
        offer(updates ${name} ${version} ${new} "${port}"
                "${port}/portfile.cmake" "${hash}" "${new_hash}"
                "${port}/vcpkg.json" "${version_text}" "${new_version_text}")
    endif ()

    if (major AND NOT MAJOR)
        offer(majors ${name} ${version} ${major} "${port}")
    endif ()
endforeach ()

function(port_version versions name out)
    string(JSON version ERROR_VARIABLE missing GET "${versions}" default ${name} baseline)

    if (missing)
        set(${out} "none" PARENT_SCOPE)
        return()
    endif ()

    string(JSON revision ERROR_VARIABLE missing GET "${versions}" default ${name} port-version)

    if (NOT missing AND revision GREATER 0)
        string(APPEND version "#${revision}")
    endif ()

    set(${out} "${version}" PARENT_SCOPE)
endfunction()

# The overview lists the direct dependencies the new baseline moves. Overlay ports pin their own version.
if (EXISTS "${root}/vcpkg.json")
    file(READ "${root}/vcpkg.json" manifest)
    string(JSON baseline ERROR_VARIABLE missing GET "${manifest}" builtin-baseline)

    if (NOT missing)
        head_of(https://github.com/microsoft/vcpkg.git refs/heads/master head)

        if (NOT head STREQUAL baseline)
            foreach (side IN ITEMS baseline head)
                file(DOWNLOAD https://raw.githubusercontent.com/microsoft/vcpkg/${${side}}/versions/baseline.json
                        "${scratch}/${side}.json" STATUS status)
                list(GET status 0 code)

                if (NOT code EQUAL 0)
                    message(FATAL_ERROR "Could not download the vcpkg baseline at ${${side}}: ${status}")
                endif ()

                file(READ "${scratch}/${side}.json" ${side}_versions)
            endforeach ()

            set(details "")
            string(JSON count LENGTH "${manifest}" dependencies)
            math(EXPR last "${count} - 1")

            foreach (index RANGE ${last})
                string(JSON type TYPE "${manifest}" dependencies ${index})

                if (type STREQUAL "OBJECT")
                    string(JSON name GET "${manifest}" dependencies ${index} name)
                else ()
                    string(JSON name GET "${manifest}" dependencies ${index})
                endif ()

                if (name IN_LIST overlay_names)
                    continue()
                endif ()

                port_version("${baseline_versions}" ${name} old)
                port_version("${head_versions}" ${name} new)

                if (NOT old STREQUAL new)
                    list(APPEND details ${name} ${old} ${new})
                endif ()
            endforeach ()

            string(SUBSTRING "${baseline}" 0 8 old)
            string(SUBSTRING "${head}" 0 8 new)
            offer(updates vcpkg ${old} ${new} vcpkg.json vcpkg.json "\"${baseline}\"" "\"${head}\"")
            set(offer_${offers}_details "${details}")
        endif ()
    endif ()
endif ()

file(REMOVE_RECURSE "${scratch}")

set(name_width 0)
set(old_width 0)
set(new_width 0)

function(widen name old new)
    foreach (column IN ITEMS name old new)
        string(LENGTH "${${column}}" length)

        if (length GREATER ${column}_width)
            set(${column}_width ${length} PARENT_SCOPE)
        endif ()
    endforeach ()
endfunction()

function(pad text width out)
    string(LENGTH "${text}" length)
    math(EXPR fill "${width} - ${length}")
    set(spaces "")

    if (fill GREATER 0)
        string(REPEAT " " ${fill} spaces)
    endif ()

    set(${out} "${text}${spaces}" PARENT_SCOPE)
endfunction()

function(show name old new where)
    pad("${name}" ${name_width} name)
    pad("${old}" ${old_width} old)

    if (where)
        pad("${new}" ${new_width} new)
        message("  ${name}  ${old}  ->  ${new}  ${where}")
    else ()
        message("  ${name}  ${old}  ->  ${new}")
    endif ()
endfunction()

foreach (index IN LISTS updates majors)
    list(GET offer_${index}_row 0 name)
    list(GET offer_${index}_row 1 old)
    list(GET offer_${index}_row 2 new)
    widen("${name}" "${old}" "${new}")
    set(details "${offer_${index}_details}")

    while (details)
        list(POP_FRONT details name old new)
        widen("  ${name}" "${old}" "")
    endwhile ()
endforeach ()

set(rest "${commits}")

while (rest)
    list(POP_FRONT rest name note)
    widen("${name}" "" "")
endwhile ()

if (updates)
    message("\nUpdates")

    foreach (index IN LISTS updates)
        show(${offer_${index}_row})
        set(details "${offer_${index}_details}")

        while (details)
            list(POP_FRONT details name old new)
            show("  ${name}" ${old} ${new} "")
        endwhile ()
    endforeach ()
endif ()

if (majors)
    message("\nMajor updates, which need -DMAJOR=ON")

    foreach (index IN LISTS majors)
        show(${offer_${index}_row})
    endforeach ()
endif ()

if (commits)
    message("\nPinned to a commit, which stays as it is")

    while (commits)
        list(POP_FRONT commits name note)
        pad("${name}" ${name_width} name)
        message("  ${name}  ${note}")
    endwhile ()
endif ()

if (unchecked)
    list(JOIN unchecked ", " unchecked)
    message("\nNot checked: ${unchecked}")
endif ()

if (NOT updates)
    message("\nNothing to update in ${project}.")
    return()
endif ()

# Quoted, since a bare YES is the true constant rather than the variable.
if (NOT "${YES}")
    message("")
    execute_process(COMMAND ${CMAKE_COMMAND} -E echo_append "Apply these updates? [y/N] ")

    if (CMAKE_HOST_WIN32)
        execute_process(COMMAND powershell -NoProfile -Command "[Console]::In.ReadLine()"
                OUTPUT_VARIABLE answer OUTPUT_STRIP_TRAILING_WHITESPACE)
    else ()
        execute_process(COMMAND head -n 1 OUTPUT_VARIABLE answer OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif ()

    if (NOT answer MATCHES "^[yY]")
        message("Nothing changed.")
        return()
    endif ()
endif ()

# Every edit is checked before any file is written, so a stale pin leaves the tree as it was.
set(touched "")

foreach (index IN LISTS updates)
    set(edits "${offer_${index}_edits}")

    while (edits)
        list(POP_FRONT edits path from to)
        string(MAKE_C_IDENTIFIER "${path}" key)

        if (NOT path IN_LIST touched)
            list(APPEND touched "${path}")
            file(READ "${root}/${path}" content_${key})
        endif ()

        string(FIND "${content_${key}}" "${from}" at)

        if (at EQUAL -1)
            message(FATAL_ERROR "${path} no longer holds ${from}, so nothing was changed")
        endif ()

        if (to MATCHES "^hash ([A-Z0-9_]+) (.+)$")
            hash_of(${CMAKE_MATCH_2} ${CMAKE_MATCH_1} to)
        endif ()

        string(REPLACE "${from}" "${to}" content_${key} "${content_${key}}")
    endwhile ()
endforeach ()

foreach (path IN LISTS touched)
    string(MAKE_C_IDENTIFIER "${path}" key)
    file(WRITE "${root}/${path}" "${content_${key}}")
endforeach ()

file(REMOVE_RECURSE "${scratch}")
list(LENGTH updates count)
message("Applied ${count} updates to ${project}. The next configure fetches the new sources.")
