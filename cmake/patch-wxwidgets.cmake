# wxWidgets 3.3.3 wraps the NSString month and weekday names from
# NSDateFormatter in wxCFStringRef, which takes ownership without retaining.
# The names are owned by the formatter, so each call over-releases them and
# the date dialog later crashes. Convert the names without taking ownership.
set(file "${CMAKE_ARGV3}/src/osx/core/uilocale.mm")
file(READ "${file}" text)
foreach(name monthName weekdayName)
    set(old "    wxCFStringRef cf(${name});\n    [df release];\n    return cf.AsString();")
    set(new "    const wxString result = wxCFStringRef::AsString(${name});\n    [df release];\n    return result;")
    string(FIND "${text}" "${old}" found)
    string(FIND "${text}" "${new}" patched)
    if(found GREATER -1)
        string(REPLACE "${old}" "${new}" text "${text}")
    elseif(patched EQUAL -1)
        message(FATAL_ERROR "wxWidgets uilocale.mm changed; review the ${name} ownership patch.")
    endif()
endforeach()
file(WRITE "${file}" "${text}")
