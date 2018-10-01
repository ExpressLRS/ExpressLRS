SelectElement("leaveCode", valueToSelect)

function SelectElement(id, valueToSelect)
{    
    var element = document.getElementById(id);
    element.value = valueToSelect;
}