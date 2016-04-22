kTableSize = 10
kStatus = 'shipping' // 'enemy_turn', 'my_turn', 'waiting'

var fillTable = function(elem) {
    var html = ''
    var base_id = elem.attr('id').split('_')[0]
    for (var i = 0; i < kTableSize; ++i) {
        html += '<tr>'
        for (var j = 0; j < kTableSize; ++j) {
           html += '<td id="' + base_id + '_' + i + '_' + j + '" state="free"></td>' 
        }
        html += '</tr>\n'
    }
    elem.html(html)
}

var coordsToTd = function(name, i, j) {
    return $('#' + name + '_' + i + '_' + j)
}

var setStatus = function(str) {
    kStatus = str
    div = $('#status_div')
    if (str == 'shipping')
        div.html('Status: ship placement')
    else if (str == 'enemy_turn')
        div.html('Status: enemy turn')
    else if (str == 'my_turn')
        div.html('Status: my turn')
    else if (str == 'waiting')
        div.html('Status: waiting')
}

var analiseAnswer = function(data) {
    name = data.split(':', 0)
    if (name == 'field1' || name == 'field2') {
        if (name == 'field1')
            name = 'my'
        else
            name = 'enemy'

        if (data.split(':')[1] == 'hit') {
            coordsToTd(name, data.split(':')[2], data.split(':')[3]).css('background_color', 'red').attr('state', 'ship')
        }
        if (data.split(':')[1] == 'miss') {
            coordsToTd(name, data.split(':')[2], data.split(':')[3]).html('<div></div>').attr('state', 'dot')
        }
        if (data.split(':')[1] == 'kill') {
            coordsToTd(name, data.split(':')[2], data.split(':')[3]).css('background_color', 'red').attr('state', 'ship')
                for (var i = 0; i < kTableSize; ++i) {
                    for (var j = 0; j < kTableSize; ++j) {
                        if (coordsToTd(name, i, j).attr('state') == 'free') {
                            if ((i > 0 && coordsToTd(name, i - 1, j).attr('state') == 'ship') ||
                                    (i < kTableSize - 1 && coordsToTd(name, i + 1, j).attr('state') == 'ship') ||
                                    (j > 0 && coordsToTd(name, i, j - 1).attr('state') == 'ship') ||
                                    (j < kTableSize - 1 && coordsToTd(name, i, j + 1).attr('state') == 'ship'))
                                coordsToTd(name, i, j).attr('state', 'dot').html('<div></div>')

                        }
                    }
                }
        }
        if (name == 'my')
            setStatus('my_turn')
    } else if (name == 'shipping') {
        if (data.split(':')[1] == 'wrong') {
            alert('Your ship placement is bad, please continue thinking')
            setStatus('shipping')
        } else {
            alert('Your ship placement is good, you can play')
            setStatus('waiting')
        }
    } else if (name == 'opponent') {
        if (data.split(':')[1] == 'came')
            alert('Opponent came')
        else if (data.split(':')[1] == 'shipped')
            alert('Opponent placed his ships')
        else if (data.split(':')[1] == 'left')
            alert('Opponent has gone away, refresh your page')
    } else if (name == 'go1') {
        alert('Your turn is first')
        setStatus('my_turn')
    } else if (name == 'go2') {
        alert('Opponent\'s turn is first')
        setStatus('enemy_turn')
    } else if (name == 'won') {
        alert ('Congratulations! You win! Refresh your page')
    } else if (name == 'lost') {
        alert ('Congratulations! You lose! Refresh your page')
    }


}

$(document).ready(function() {
    console.log('ready') 
    fillTable($('#my_table'))
    fillTable($('#enemy_table'))
    setStatus('shipping')
    setInterval(function() {
        $.post('/', '', function(data) {
            analiseAnswer(data) // TODO delete comment
        })
    }, 2000) 
})

$(document).on('click', '#my_table td', function() {
    if (kStatus == 'shipping') {
        if ($(this).attr('state') == 'free') {
            $(this).attr('state', 'ship').css('background-color', 'blue')
        } else {
            $(this).attr('state', 'free').css('background-color', 'white')
            // $(this).html('<div></div>')
        }
    }
})

$(document).on('click', '#enemy_table td', function() {
    if (kStatus == 'my_turn') {
        if ($(this).attr('state') == 'free') {
            x = $(this).attr('id').split('_')[1]
            y = $(this).attr('id').split('_')[2]
            setStatus('enemy_turn')
            $.post('/', 'turn:' + x + ':' + y, function(data) {
                analiseAnswer(data)
            })
        } else {
            alert('You must not press here')
        }
    }
})

$(document).on('click', '#send_ships', function() {
    if (kStatus != 'shipping') {
        alert('You must not press here')
        return
    }
    setStatus('waiting')
    ships_msg = 'shipping:'
    for (var i = 0; i < kTableSize; ++i) {
        for (var j = 0; j < kTableSize; ++j) {
            if (coordsToTd('my', i, j).attr('state') == 'ship')
                ships_msg += '1'
            else
                ships_msg += '0'
        }
    }
    
    $.post('/', '', function(data) {
        analiseAnswer(data)
    })
})
