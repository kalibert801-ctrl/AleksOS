param([string]$message = "Update")

Write-Host "📝 Добавляю изменения..." -ForegroundColor Cyan
git add .

Write-Host "💾 Создаю commit: '$message'" -ForegroundColor Cyan
git commit -m $message

Write-Host "🚀 Отправляю на GitHub..." -ForegroundColor Cyan
git push

Write-Host "✅ Готово!" -ForegroundColor Green
