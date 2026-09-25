if(NOT DEFINED JUCE_SOURCE_DIR)
    message(FATAL_ERROR "JUCE_SOURCE_DIR was not provided")
endif()

set(header "${JUCE_SOURCE_DIR}/modules/juce_gui_extra/misc/juce_WebBrowserComponent.h")
set(windows_source "${JUCE_SOURCE_DIR}/modules/juce_gui_extra/native/juce_WebBrowserComponent_windows.cpp")

file(READ "${header}" header_text)
if(NOT header_text MATCHES "FileDropListener")
    string(REPLACE
        "using NativeEventListener = std::function<void (var)>;"
        "using NativeEventListener = std::function<void (var)>;\n    using FileDropListener = std::function<void (const StringArray&)>;"
        header_text "${header_text}")
    string(REPLACE
        "const auto& getEventListeners() const                            { return eventListeners; }"
        "[[nodiscard]] Options withFileDropListener (FileDropListener listener) const { return withMember (*this, &Options::fileDropListener, std::move (listener)); }\n\n        const auto& getEventListeners() const                            { return eventListeners; }\n        const auto& getFileDropListener() const                          { return fileDropListener; }"
        header_text "${header_text}")
    string(REPLACE
        "std::vector<std::pair<Identifier, NativeEventListener>> eventListeners;"
        "std::vector<std::pair<Identifier, NativeEventListener>> eventListeners;\n        FileDropListener fileDropListener;"
        header_text "${header_text}")
    if(NOT header_text MATCHES "withFileDropListener")
        message(FATAL_ERROR "Could not patch JUCE WebBrowserComponent options")
    endif()
    file(WRITE "${header}" "${header_text}")
endif()

file(READ "${windows_source}" windows_text)
if(NOT windows_text MATCHES "__recompilerFileDrop")
    set(old_handler [[            webView->add_WebMessageReceived (Callback<ICoreWebView2WebMessageReceivedEventHandler> (
                                                 [this] (ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT
                                                 {
                                                     if (const auto str = callMethodWithLpwstrResult (args, &ICoreWebView2WebMessageReceivedEventArgs::TryGetWebMessageAsString))
                                                         owner.impl->handleNativeEvent (JSON::fromString (*str));

                                                     return S_OK;
                                                 }).Get(), &webMessageReceivedToken);]])
    set(new_handler [[            webView->add_WebMessageReceived (Callback<ICoreWebView2WebMessageReceivedEventHandler> (
                                                 [this] (ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT
                                                 {
                                                     const auto str = callMethodWithLpwstrResult (args, &ICoreWebView2WebMessageReceivedEventArgs::TryGetWebMessageAsString);
                                                     const auto source = getUriStringFromArgs (args);
                                                     if (str == "__recompilerFileDrop"
                                                         && source.startsWith (WebBrowserComponent::getResourceProviderRoot()))
                                                     {
                                                         StringArray paths;
                                                         ComPtr<ICoreWebView2WebMessageReceivedEventArgs2> args2;
                                                         ComPtr<ICoreWebView2ObjectCollectionView> objects;
                                                         if (SUCCEEDED (args->QueryInterface (IID_PPV_ARGS (&args2)))
                                                             && args2 != nullptr
                                                             && SUCCEEDED (args2->get_AdditionalObjects (&objects))
                                                             && objects != nullptr)
                                                         {
                                                             UINT32 count = 0;
                                                             objects->get_Count (&count);
                                                             for (UINT32 index = 0; index < count; ++index)
                                                             {
                                                                 ComPtr<IUnknown> object;
                                                                 ComPtr<ICoreWebView2File> file;
                                                                 if (SUCCEEDED (objects->GetValueAtIndex (index, &object))
                                                                     && object != nullptr
                                                                     && SUCCEEDED (object.As (&file))
                                                                     && file != nullptr)
                                                                 {
                                                                     LPWSTR path = nullptr;
                                                                     if (SUCCEEDED (file->get_Path (&path)) && path != nullptr)
                                                                     {
                                                                         paths.add (String { CharPointer_UTF16 { path } });
                                                                         CoTaskMemFree (path);
                                                                     }
                                                                 }
                                                             }
                                                         }
                                                         if (const auto& listener = preferences.getFileDropListener(); listener != nullptr)
                                                             listener (paths);
                                                     }
                                                     else if (str)
                                                     {
                                                         owner.impl->handleNativeEvent (JSON::fromString (*str));
                                                     }
                                                     return S_OK;
                                                 }).Get(), &webMessageReceivedToken);]])
    string(REPLACE "${old_handler}" "${new_handler}" windows_text "${windows_text}")
    if(NOT windows_text MATCHES "__recompilerFileDrop")
        message(FATAL_ERROR "Could not patch JUCE WebView2 message handler")
    endif()
    file(WRITE "${windows_source}" "${windows_text}")
endif()
