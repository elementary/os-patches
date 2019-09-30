function onTabCreated (tab) {

}

function onTabRemoved (tabid) {

}

function onTabUpdated (tabid, changeInfo, tab) {

}

chrome.tabs.onUpdated.addListener (onTabUpdated);
chrome.tabs.onCreated.addListener (onTabCreated);
chrome.tabs.onRemoved.addListener (onTabRemoved);
